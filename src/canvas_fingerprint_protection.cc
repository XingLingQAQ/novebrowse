#include "novebrowse/canvas_fingerprint_protection.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "novebrowse/fingerprint_manager.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace novebrowse {

// Static member definitions
std::unordered_map<std::string, CanvasFingerprintDetector::CanvasUsageStats> 
    CanvasFingerprintDetector::canvas_stats_;
base::Lock CanvasFingerprintDetector::stats_lock_;

// static
bool CanvasFingerprintProtection::IsEnabled() {
  return FingerprintManager::IsEnabled();
}

// static
blink::ImageData* CanvasFingerprintProtection::ProcessImageData(
    blink::ImageData* original_data,
    blink::CanvasRenderingContextHost* host) {
  if (!IsEnabled() || !original_data || !host) {
    return original_data;
  }
  
  CanvasConfig config = GetConfigForHost(host);
  if (!config.enabled || !config.protect_image_data) {
    return original_data;
  }
  
  // Record operation for detection
  CanvasFingerprintDetector::RecordCanvasOperation(host, "getImageData", "");
  
  // Generate noise seed based on host
  uint32_t seed = GenerateNoiseSeed(host);
  
  // Apply noise to image data
  blink::DOMUint8ClampedArray* data_array = original_data->data();
  if (data_array && config.add_noise) {
    uint8_t* pixel_data = data_array->Data();
    size_t data_length = data_array->length();
    
    ProcessPixelData(pixel_data, data_length, config);
  }
  
  INCREMENT_FINGERPRINT_STAT("canvas_operations_spoofed");
  return original_data;
}

// static
WTF::String CanvasFingerprintProtection::ProcessDataURL(
    const WTF::String& original_url,
    blink::CanvasRenderingContextHost* host) {
  if (!IsEnabled() || !host) {
    return original_url;
  }
  
  CanvasConfig config = GetConfigForHost(host);
  if (!config.enabled || !config.protect_data_url) {
    return original_url;
  }
  
  // Record operation for detection
  CanvasFingerprintDetector::RecordCanvasOperation(host, "toDataURL", "");
  
  // For data URLs, we modify the canvas before export
  // This is handled at the canvas level, so we just record the operation
  INCREMENT_FINGERPRINT_STAT("canvas_operations_spoofed");
  return original_url;
}

// static
blink::TextMetrics* CanvasFingerprintProtection::ProcessTextMetrics(
    blink::TextMetrics* original_metrics,
    blink::CanvasRenderingContextHost* host) {
  if (!IsEnabled() || !original_metrics || !host) {
    return original_metrics;
  }
  
  CanvasConfig config = GetConfigForHost(host);
  if (!config.enabled || !config.spoof_text_metrics) {
    return original_metrics;
  }
  
  // Record operation for detection
  CanvasFingerprintDetector::RecordCanvasOperation(host, "measureText", "");
  
  // Apply slight offsets to text metrics
  ApplyTextMetricsOffset(original_metrics, config);
  
  INCREMENT_FINGERPRINT_STAT("canvas_operations_spoofed");
  return original_metrics;
}

// static
void CanvasFingerprintProtection::ProcessPixelData(
    uint8_t* pixel_data,
    size_t data_length,
    const CanvasConfig& config) {
  if (!pixel_data || data_length == 0 || !config.add_noise) {
    return;
  }
  
  // Generate deterministic noise based on pixel data hash
  std::hash<std::string> hasher;
  std::string data_str(reinterpret_cast<char*>(pixel_data), 
                       std::min(data_length, size_t(1024)));
  uint32_t seed = static_cast<uint32_t>(hasher(data_str));
  
  CanvasNoiseGenerator noise_generator(seed);
  
  // Process pixels in RGBA format
  for (size_t i = 0; i < data_length; i += 4) {
    if (i + 3 < data_length) {
      int x = (i / 4) % 1000; // Assume max width of 1000 for noise calculation
      int y = (i / 4) / 1000;
      
      // Apply noise to RGB channels, leave alpha unchanged
      pixel_data[i] = noise_generator.GeneratePixelNoise(x, y, 0, pixel_data[i], config.noise_level);
      pixel_data[i + 1] = noise_generator.GeneratePixelNoise(x, y, 1, pixel_data[i + 1], config.noise_level);
      pixel_data[i + 2] = noise_generator.GeneratePixelNoise(x, y, 2, pixel_data[i + 2], config.noise_level);
      // pixel_data[i + 3] is alpha, keep unchanged
    }
  }
}

// static
void CanvasFingerprintProtection::AddCanvasNoise(
    SkBitmap& bitmap,
    double noise_level) {
  if (bitmap.empty() || noise_level <= 0.0) {
    return;
  }
  
  SkImageInfo info = bitmap.info();
  if (info.colorType() != kRGBA_8888_SkColorType && 
      info.colorType() != kBGRA_8888_SkColorType) {
    return;
  }
  
  SkPixmap pixmap;
  if (!bitmap.peekPixels(&pixmap)) {
    return;
  }
  
  uint8_t* pixels = static_cast<uint8_t*>(pixmap.writable_addr());
  size_t row_bytes = pixmap.rowBytes();
  
  // Generate noise seed from bitmap content
  uint32_t seed = 0;
  for (int y = 0; y < info.height() && y < 10; ++y) {
    for (int x = 0; x < info.width() && x < 10; ++x) {
      uint8_t* pixel = pixels + y * row_bytes + x * info.bytesPerPixel();
      seed ^= (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | pixel[3];
    }
  }
  
  CanvasNoiseGenerator noise_generator(seed);
  
  // Apply noise to each pixel
  for (int y = 0; y < info.height(); ++y) {
    for (int x = 0; x < info.width(); ++x) {
      uint8_t* pixel = pixels + y * row_bytes + x * info.bytesPerPixel();
      
      // Apply noise to RGB channels
      pixel[0] = noise_generator.GeneratePixelNoise(x, y, 0, pixel[0], noise_level);
      pixel[1] = noise_generator.GeneratePixelNoise(x, y, 1, pixel[1], noise_level);
      pixel[2] = noise_generator.GeneratePixelNoise(x, y, 2, pixel[2], noise_level);
      // Keep alpha channel unchanged
    }
  }
}

// static
uint32_t CanvasFingerprintProtection::GenerateNoiseSeed(
    blink::CanvasRenderingContextHost* host) {
  if (!host) {
    return 12345; // Default seed
  }
  
  // Generate seed based on canvas properties
  std::string canvas_id = CalculateCanvasHash(host);
  std::hash<std::string> hasher;
  return static_cast<uint32_t>(hasher(canvas_id));
}

// static
CanvasConfig CanvasFingerprintProtection::GetConfigForHost(
    blink::CanvasRenderingContextHost* host) {
  if (!host) {
    return CanvasConfig{};
  }
  
  // Get config from fingerprint manager
  FingerprintConfig config = FINGERPRINT_MANAGER()->GetDefaultConfig();
  return config.canvas;
}

// static
double CanvasFingerprintProtection::GenerateNoise(uint32_t seed, int x, int y, int channel) {
  // Simple deterministic noise function
  uint32_t hash = seed;
  hash ^= (x * 73856093) ^ (y * 19349663) ^ (channel * 83492791);
  hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
  hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
  hash = (hash >> 16) ^ hash;
  
  return (static_cast<double>(hash % 1000) / 1000.0 - 0.5) * 2.0; // Range [-1, 1]
}

// static
void CanvasFingerprintProtection::ApplyPixelNoise(
    uint8_t* pixel,
    uint32_t seed,
    int x,
    int y,
    double noise_level) {
  for (int channel = 0; channel < 3; ++channel) {
    double noise = GenerateNoise(seed, x, y, channel) * noise_level * 255.0;
    int new_value = static_cast<int>(pixel[channel]) + static_cast<int>(noise);
    pixel[channel] = static_cast<uint8_t>(std::max(0, std::min(255, new_value)));
  }
}

// static
void CanvasFingerprintProtection::ApplyTextMetricsOffset(
    blink::TextMetrics* metrics,
    const CanvasConfig& config) {
  if (!metrics || !config.spoof_text_metrics) {
    return;
  }
  
  // Apply small random offsets to text metrics
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<double> dis(-0.5, 0.5);
  
  // Modify width slightly
  double width_offset = dis(gen) * 0.1; // ±0.05 pixel offset
  metrics->setWidth(metrics->width() + width_offset);
  
  // Modify other metrics with smaller offsets
  double small_offset = dis(gen) * 0.05;
  metrics->setActualBoundingBoxLeft(metrics->actualBoundingBoxLeft() + small_offset);
  metrics->setActualBoundingBoxRight(metrics->actualBoundingBoxRight() + small_offset);
  metrics->setActualBoundingBoxAscent(metrics->actualBoundingBoxAscent() + small_offset);
  metrics->setActualBoundingBoxDescent(metrics->actualBoundingBoxDescent() + small_offset);
}

// static
std::string CanvasFingerprintProtection::CalculateCanvasHash(
    blink::CanvasRenderingContextHost* host) {
  if (!host) {
    return "null_canvas";
  }
  
  // Generate a hash based on canvas properties
  std::stringstream ss;
  ss << "canvas_" << reinterpret_cast<uintptr_t>(host);
  
  return ss.str();
}

// CanvasNoiseGenerator implementation
CanvasNoiseGenerator::CanvasNoiseGenerator(uint32_t seed) : seed_(seed) {}

uint8_t CanvasNoiseGenerator::GeneratePixelNoise(int x, int y, int channel, 
                                                uint8_t original_value, 
                                                double noise_level) {
  if (noise_level <= 0.0) {
    return original_value;
  }
  
  double noise = PerlinNoise(x * 0.1, y * 0.1) * noise_level * 10.0;
  int new_value = static_cast<int>(original_value) + static_cast<int>(noise);
  return static_cast<uint8_t>(std::max(0, std::min(255, new_value)));
}

double CanvasNoiseGenerator::GenerateFloatNoise(int x, int y, double noise_level) {
  if (noise_level <= 0.0) {
    return 0.0;
  }
  
  return PerlinNoise(x * 0.1, y * 0.1) * noise_level;
}

void CanvasNoiseGenerator::SetSeed(uint32_t seed) {
  seed_ = seed;
}

uint32_t CanvasNoiseGenerator::NextRandom() {
  seed_ = (seed_ * 1103515245 + 12345) & 0x7fffffff;
  return seed_;
}

double CanvasNoiseGenerator::NextFloat() {
  return static_cast<double>(NextRandom()) / static_cast<double>(0x7fffffff);
}

double CanvasNoiseGenerator::PerlinNoise(double x, double y) {
  // Simplified Perlin noise implementation
  int xi = static_cast<int>(x) & 255;
  int yi = static_cast<int>(y) & 255;
  
  double xf = x - static_cast<int>(x);
  double yf = y - static_cast<int>(y);
  
  double u = Fade(xf);
  double v = Fade(yf);
  
  int aa = (xi + yi) & 255;
  int ab = (xi + yi + 1) & 255;
  int ba = (xi + 1 + yi) & 255;
  int bb = (xi + 1 + yi + 1) & 255;
  
  double x1 = Lerp(Grad(aa, xf, yf), Grad(ba, xf - 1, yf), u);
  double x2 = Lerp(Grad(ab, xf, yf - 1), Grad(bb, xf - 1, yf - 1), u);
  
  return Lerp(x1, x2, v);
}

double CanvasNoiseGenerator::Fade(double t) {
  return t * t * t * (t * (t * 6 - 15) + 10);
}

double CanvasNoiseGenerator::Lerp(double t, double a, double b) {
  return a + t * (b - a);
}

double CanvasNoiseGenerator::Grad(int hash, double x, double y) {
  int h = hash & 15;
  double u = h < 8 ? x : y;
  double v = h < 4 ? y : h == 12 || h == 14 ? x : 0;
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

// CanvasFingerprintDetector implementation
// static
bool CanvasFingerprintDetector::DetectFingerprintingAttempt(
    blink::CanvasRenderingContextHost* host,
    const std::string& operation) {
  if (!host) {
    return false;
  }
  
  RecordCanvasOperation(host, operation, "");
  return AnalyzeUsagePattern(host);
}

// static
void CanvasFingerprintDetector::RecordCanvasOperation(
    blink::CanvasRenderingContextHost* host,
    const std::string& operation,
    const std::string& parameters) {
  if (!host) {
    return;
  }
  
  base::AutoLock auto_lock(stats_lock_);
  
  std::string canvas_id = base::StringPrintf("canvas_%p", host);
  CanvasUsageStats& stats = canvas_stats_[canvas_id];
  
  int64_t current_time = base::Time::Now().ToInternalValue();
  if (stats.first_operation_time == 0) {
    stats.first_operation_time = current_time;
  }
  stats.last_operation_time = current_time;
  
  stats.operation_sequence.push_back(operation);
  
  // Limit sequence length to prevent memory bloat
  if (stats.operation_sequence.size() > 100) {
    stats.operation_sequence.erase(stats.operation_sequence.begin());
  }
  
  // Categorize operations
  if (operation == "fillRect" || operation == "strokeRect" || 
      operation == "fillText" || operation == "strokeText" ||
      operation == "drawImage") {
    stats.draw_operations++;
  } else if (operation == "getImageData" || operation == "toDataURL") {
    stats.read_operations++;
    if (operation == "getImageData") {
      stats.image_data_reads++;
    } else {
      stats.data_url_exports++;
    }
  } else if (operation == "fillText" || operation == "strokeText" || 
             operation == "measureText") {
    stats.text_operations++;
  }
}

// static
bool CanvasFingerprintDetector::AnalyzeUsagePattern(
    blink::CanvasRenderingContextHost* host) {
  if (!host) {
    return false;
  }
  
  base::AutoLock auto_lock(stats_lock_);
  
  std::string canvas_id = base::StringPrintf("canvas_%p", host);
  auto it = canvas_stats_.find(canvas_id);
  if (it == canvas_stats_.end()) {
    return false;
  }
  
  const CanvasUsageStats& stats = it->second;
  return IsLikelyFingerprintingPattern(stats);
}

// static
bool CanvasFingerprintDetector::IsLikelyFingerprintingPattern(
    const CanvasUsageStats& stats) {
  // Check for suspicious patterns
  if (HasHighReadToWriteRatio(stats)) {
    return true;
  }
  
  if (HasSuspiciousOperationSequence(stats.operation_sequence)) {
    return true;
  }
  
  // Check for rapid canvas operations (potential automated fingerprinting)
  if (stats.first_operation_time != 0 && stats.last_operation_time != 0) {
    int64_t duration = stats.last_operation_time - stats.first_operation_time;
    int total_operations = stats.draw_operations + stats.read_operations;
    
    if (total_operations > 10 && duration < 1000000) { // Less than 1 second
      return true;
    }
  }
  
  return false;
}

// static
bool CanvasFingerprintDetector::HasSuspiciousOperationSequence(
    const std::vector<std::string>& sequence) {
  if (sequence.size() < 3) {
    return false;
  }
  
  // Look for patterns like: draw -> read -> draw -> read (fingerprinting)
  int consecutive_reads = 0;
  int total_reads = 0;
  
  for (const auto& op : sequence) {
    if (op == "getImageData" || op == "toDataURL") {
      consecutive_reads++;
      total_reads++;
    } else {
      consecutive_reads = 0;
    }
    
    // Multiple consecutive reads is suspicious
    if (consecutive_reads > 2) {
      return true;
    }
  }
  
  // High percentage of read operations is suspicious
  double read_ratio = static_cast<double>(total_reads) / sequence.size();
  return read_ratio > 0.5;
}

// static
bool CanvasFingerprintDetector::HasHighReadToWriteRatio(
    const CanvasUsageStats& stats) {
  if (stats.draw_operations == 0) {
    return stats.read_operations > 0; // Only reads, no draws
  }
  
  double ratio = static_cast<double>(stats.read_operations) / stats.draw_operations;
  return ratio > 2.0; // More than 2 reads per draw operation
}

}  // namespace novebrowse
