#include "novebrowse/webgl_fingerprint_protection.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "novebrowse/fingerprint_manager.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "v8/include/v8.h"

namespace novebrowse {

// Static member definitions
const std::unordered_map<GLenum, std::string> WebGLFingerprintProtection::kParameterNames = {
  {GL_VENDOR, "VENDOR"},
  {GL_RENDERER, "RENDERER"},
  {GL_VERSION, "VERSION"},
  {GL_SHADING_LANGUAGE_VERSION, "SHADING_LANGUAGE_VERSION"},
  {GL_MAX_TEXTURE_SIZE, "MAX_TEXTURE_SIZE"},
  {GL_MAX_CUBE_MAP_TEXTURE_SIZE, "MAX_CUBE_MAP_TEXTURE_SIZE"},
  {GL_MAX_RENDERBUFFER_SIZE, "MAX_RENDERBUFFER_SIZE"},
  {GL_MAX_VIEWPORT_DIMS, "MAX_VIEWPORT_DIMS"},
  {GL_MAX_VERTEX_ATTRIBS, "MAX_VERTEX_ATTRIBS"},
  {GL_MAX_VERTEX_UNIFORM_VECTORS, "MAX_VERTEX_UNIFORM_VECTORS"},
  {GL_MAX_FRAGMENT_UNIFORM_VECTORS, "MAX_FRAGMENT_UNIFORM_VECTORS"},
  {GL_MAX_VARYING_VECTORS, "MAX_VARYING_VECTORS"}
};

const std::unordered_map<GLenum, std::string> WebGLFingerprintProtection::kDefaultStringValues = {
  {GL_VENDOR, "WebKit"},
  {GL_RENDERER, "WebKit WebGL"},
  {GL_VERSION, "OpenGL ES 2.0 (WebKit)"},
  {GL_SHADING_LANGUAGE_VERSION, "OpenGL ES GLSL ES 1.00 (WebKit)"}
};

const std::unordered_map<GLenum, std::vector<GLint>> WebGLFingerprintProtection::kDefaultIntArrayValues = {
  {GL_MAX_VIEWPORT_DIMS, {16384, 16384}},
  {GL_ALIASED_LINE_WIDTH_RANGE, {1, 1}},
  {GL_ALIASED_POINT_SIZE_RANGE, {1, 1024}}
};

const std::unordered_map<GLenum, std::vector<GLfloat>> WebGLFingerprintProtection::kDefaultFloatArrayValues = {
  {GL_DEPTH_RANGE, {0.0f, 1.0f}},
  {GL_COLOR_CLEAR_VALUE, {0.0f, 0.0f, 0.0f, 0.0f}}
};

std::unordered_map<std::string, WebGLFingerprintDetector::WebGLUsageStats> 
    WebGLFingerprintDetector::webgl_stats_;
base::Lock WebGLFingerprintDetector::stats_lock_;

// static
bool WebGLFingerprintProtection::IsEnabled() {
  return FingerprintManager::IsEnabled();
}

// static
std::optional<v8::Local<v8::Value>> WebGLFingerprintProtection::GetSpoofedParameter(
    GLenum pname,
    blink::WebGLRenderingContextBase* context) {
  if (!IsEnabled() || !context) {
    return std::nullopt;
  }
  
  WebGLConfig config = GetConfigForContext(context);
  if (!config.enabled) {
    return std::nullopt;
  }
  
  // Record parameter query for detection
  WebGLFingerprintDetector::RecordWebGLOperation(context, "getParameter", 
                                                 base::StringPrintf("0x%x", pname));
  
  v8::Isolate* isolate = context->GetScriptState()->GetIsolate();
  
  switch (pname) {
    case GL_VENDOR:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return CreateSpoofedVendor(isolate, config);
      
    case GL_RENDERER:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return CreateSpoofedRenderer(isolate, config);
      
    case GL_VERSION:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return CreateSpoofedVersion(isolate, config);
      
    case GL_SHADING_LANGUAGE_VERSION:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return CreateSpoofedShadingLanguageVersion(isolate, config);
      
    case GL_MAX_TEXTURE_SIZE:
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
    case GL_MAX_RENDERBUFFER_SIZE:
    case GL_MAX_VERTEX_ATTRIBS:
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
    case GL_MAX_VARYING_VECTORS:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return CreateSpoofedParameter(isolate, pname, config);
      
    case GL_MAX_VIEWPORT_DIMS: {
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      v8::Local<v8::Array> array = v8::Array::New(isolate, 2);
      array->Set(context->GetScriptState()->GetContext(), 0, v8::Integer::New(isolate, 16384)).Check();
      array->Set(context->GetScriptState()->GetContext(), 1, v8::Integer::New(isolate, 16384)).Check();
      return array;
    }
      
    default:
      return std::nullopt;
  }
}

// static
WTF::String WebGLFingerprintProtection::GetSpoofedStringParameter(
    GLenum pname,
    blink::WebGLRenderingContextBase* context) {
  if (!IsEnabled() || !context) {
    return WTF::String();
  }
  
  WebGLConfig config = GetConfigForContext(context);
  if (!config.enabled) {
    return WTF::String();
  }
  
  // Record parameter query for detection
  WebGLFingerprintDetector::RecordWebGLOperation(context, "getParameter", 
                                                 base::StringPrintf("0x%x", pname));
  
  switch (pname) {
    case GL_VENDOR:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return WTF::String::FromUTF8(config.vendor.c_str());
      
    case GL_RENDERER:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return WTF::String::FromUTF8(config.renderer.c_str());
      
    case GL_VERSION:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return WTF::String::FromUTF8(config.version.c_str());
      
    case GL_SHADING_LANGUAGE_VERSION:
      INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
      return WTF::String::FromUTF8(config.shading_language_version.c_str());
      
    default:
      return WTF::String();
  }
}

// static
void WebGLFingerprintProtection::ProcessBufferData(
    void* buffer_data,
    size_t data_size,
    const WebGLConfig& config) {
  if (!buffer_data || data_size == 0 || !config.add_noise_to_buffers) {
    return;
  }
  
  // Generate seed from buffer content
  std::hash<std::string> hasher;
  std::string data_str(reinterpret_cast<char*>(buffer_data), 
                       std::min(data_size, size_t(256)));
  uint32_t seed = static_cast<uint32_t>(hasher(data_str));
  
  ApplyBufferNoise(buffer_data, data_size, seed, config.buffer_noise_level);
}

// static
std::vector<std::string> WebGLFingerprintProtection::GetSpoofedExtensions(
    blink::WebGLRenderingContextBase* context) {
  if (!IsEnabled() || !context) {
    return {};
  }
  
  WebGLConfig config = GetConfigForContext(context);
  if (!config.enabled) {
    return {};
  }
  
  // Record extension query for detection
  WebGLFingerprintDetector::RecordWebGLOperation(context, "getSupportedExtensions", "");
  
  INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
  return config.extensions;
}

// static
void WebGLFingerprintProtection::GetSpoofedShaderPrecisionFormat(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision) {
  if (!IsEnabled() || !range || !precision) {
    return;
  }
  
  // Provide consistent precision format values
  switch (precisiontype) {
    case GL_LOW_FLOAT:
      range[0] = 127;
      range[1] = 127;
      *precision = 23;
      break;
    case GL_MEDIUM_FLOAT:
      range[0] = 127;
      range[1] = 127;
      *precision = 23;
      break;
    case GL_HIGH_FLOAT:
      range[0] = 127;
      range[1] = 127;
      *precision = 23;
      break;
    case GL_LOW_INT:
      range[0] = 31;
      range[1] = 30;
      *precision = 0;
      break;
    case GL_MEDIUM_INT:
      range[0] = 31;
      range[1] = 30;
      *precision = 0;
      break;
    case GL_HIGH_INT:
      range[0] = 31;
      range[1] = 30;
      *precision = 0;
      break;
    default:
      range[0] = 0;
      range[1] = 0;
      *precision = 0;
      break;
  }
  
  INCREMENT_FINGERPRINT_STAT("webgl_parameters_spoofed");
}

// static
void WebGLFingerprintProtection::ProcessTextureData(
    const void* pixels,
    GLenum format,
    GLenum type,
    GLsizei width,
    GLsizei height,
    const WebGLConfig& config) {
  if (!pixels || !config.add_noise_to_buffers) {
    return;
  }
  
  // Calculate data size based on format and type
  size_t bytes_per_pixel = 0;
  switch (format) {
    case GL_RGB:
      bytes_per_pixel = 3;
      break;
    case GL_RGBA:
      bytes_per_pixel = 4;
      break;
    case GL_LUMINANCE:
    case GL_ALPHA:
      bytes_per_pixel = 1;
      break;
    case GL_LUMINANCE_ALPHA:
      bytes_per_pixel = 2;
      break;
    default:
      return;
  }
  
  size_t data_size = width * height * bytes_per_pixel;
  
  // Generate seed from texture properties
  uint32_t seed = (width << 16) | height | (format << 8) | type;
  
  // Apply noise to texture data
  ApplyBufferNoise(const_cast<void*>(pixels), data_size, seed, config.buffer_noise_level);
}

// static
WebGLConfig WebGLFingerprintProtection::GetConfigForContext(
    blink::WebGLRenderingContextBase* context) {
  if (!context) {
    return WebGLConfig{};
  }
  
  // Get config from fingerprint manager
  FingerprintConfig config = FINGERPRINT_MANAGER()->GetDefaultConfig();
  return config.webgl;
}

// static
std::string WebGLFingerprintProtection::GetContextId(
    blink::WebGLRenderingContextBase* context) {
  if (!context) {
    return "null_context";
  }
  
  return base::StringPrintf("webgl_%p", context);
}

// static
uint32_t WebGLFingerprintProtection::GenerateNoiseSeed(
    blink::WebGLRenderingContextBase* context) {
  std::string context_id = GetContextId(context);
  std::hash<std::string> hasher;
  return static_cast<uint32_t>(hasher(context_id));
}

// static
void WebGLFingerprintProtection::ApplyBufferNoise(
    void* buffer,
    size_t size,
    uint32_t seed,
    double noise_level) {
  if (!buffer || size == 0 || noise_level <= 0.0) {
    return;
  }
  
  WebGLNoiseGenerator noise_generator(seed);
  noise_generator.GenerateBufferNoise(buffer, size, noise_level);
}

// static
v8::Local<v8::Value> WebGLFingerprintProtection::CreateSpoofedVendor(
    v8::Isolate* isolate,
    const WebGLConfig& config) {
  return v8::String::NewFromUtf8(isolate, config.vendor.c_str()).ToLocalChecked();
}

// static
v8::Local<v8::Value> WebGLFingerprintProtection::CreateSpoofedRenderer(
    v8::Isolate* isolate,
    const WebGLConfig& config) {
  return v8::String::NewFromUtf8(isolate, config.renderer.c_str()).ToLocalChecked();
}

// static
v8::Local<v8::Value> WebGLFingerprintProtection::CreateSpoofedVersion(
    v8::Isolate* isolate,
    const WebGLConfig& config) {
  return v8::String::NewFromUtf8(isolate, config.version.c_str()).ToLocalChecked();
}

// static
v8::Local<v8::Value> WebGLFingerprintProtection::CreateSpoofedShadingLanguageVersion(
    v8::Isolate* isolate,
    const WebGLConfig& config) {
  return v8::String::NewFromUtf8(isolate, config.shading_language_version.c_str()).ToLocalChecked();
}

// static
v8::Local<v8::Value> WebGLFingerprintProtection::CreateSpoofedExtensions(
    v8::Isolate* isolate,
    const WebGLConfig& config) {
  v8::Local<v8::Array> extensions_array = v8::Array::New(isolate, config.extensions.size());
  
  for (size_t i = 0; i < config.extensions.size(); ++i) {
    v8::Local<v8::String> extension = v8::String::NewFromUtf8(
        isolate, config.extensions[i].c_str()).ToLocalChecked();
    extensions_array->Set(isolate->GetCurrentContext(), i, extension).Check();
  }
  
  return extensions_array;
}

// static
v8::Local<v8::Value> WebGLFingerprintProtection::CreateSpoofedParameter(
    v8::Isolate* isolate,
    GLenum pname,
    const WebGLConfig& config) {
  // Check if we have a custom parameter value
  auto param_name_it = kParameterNames.find(pname);
  if (param_name_it != kParameterNames.end()) {
    auto param_it = config.parameters.find(param_name_it->second);
    if (param_it != config.parameters.end()) {
      // Try to parse as integer first
      int int_value;
      if (base::StringToInt(param_it->second, &int_value)) {
        return v8::Integer::New(isolate, int_value);
      }
      
      // Otherwise return as string
      return v8::String::NewFromUtf8(isolate, param_it->second.c_str()).ToLocalChecked();
    }
  }
  
  // Return default values for common parameters
  switch (pname) {
    case GL_MAX_TEXTURE_SIZE:
      return v8::Integer::New(isolate, 16384);
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return v8::Integer::New(isolate, 16384);
    case GL_MAX_RENDERBUFFER_SIZE:
      return v8::Integer::New(isolate, 16384);
    case GL_MAX_VERTEX_ATTRIBS:
      return v8::Integer::New(isolate, 16);
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      return v8::Integer::New(isolate, 1024);
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return v8::Integer::New(isolate, 1024);
    case GL_MAX_VARYING_VECTORS:
      return v8::Integer::New(isolate, 30);
    default:
      return v8::Integer::New(isolate, 0);
  }
}

// WebGLNoiseGenerator implementation
WebGLNoiseGenerator::WebGLNoiseGenerator(uint32_t seed) : seed_(seed) {}

void WebGLNoiseGenerator::GenerateBufferNoise(void* buffer, size_t size, double noise_level) {
  if (!buffer || size == 0 || noise_level <= 0.0) {
    return;
  }
  
  uint8_t* byte_buffer = static_cast<uint8_t*>(buffer);
  
  for (size_t i = 0; i < size; ++i) {
    float noise = GenerateFloatNoise(byte_buffer[i], noise_level);
    int new_value = static_cast<int>(byte_buffer[i]) + static_cast<int>(noise * 255.0f);
    byte_buffer[i] = static_cast<uint8_t>(std::max(0, std::min(255, new_value)));
  }
}

float WebGLNoiseGenerator::GenerateFloatNoise(float original_value, double noise_level) {
  if (noise_level <= 0.0) {
    return original_value;
  }
  
  float noise = (NextFloat() - 0.5f) * 2.0f * static_cast<float>(noise_level);
  return original_value + noise;
}

int WebGLNoiseGenerator::GenerateIntNoise(int original_value, double noise_level) {
  if (noise_level <= 0.0) {
    return original_value;
  }
  
  int noise = static_cast<int>((NextFloat() - 0.5f) * 2.0f * noise_level * 100.0f);
  return original_value + noise;
}

void WebGLNoiseGenerator::SetSeed(uint32_t seed) {
  seed_ = seed;
}

uint32_t WebGLNoiseGenerator::NextRandom() {
  seed_ = (seed_ * 1103515245 + 12345) & 0x7fffffff;
  return seed_;
}

float WebGLNoiseGenerator::NextFloat() {
  return static_cast<float>(NextRandom()) / static_cast<float>(0x7fffffff);
}

double WebGLNoiseGenerator::NextGaussian() {
  static bool has_spare = false;
  static double spare;
  
  if (has_spare) {
    has_spare = false;
    return spare;
  }
  
  has_spare = true;
  double u = NextFloat();
  double v = NextFloat();
  double mag = sqrt(-2.0 * log(u));
  spare = mag * cos(2.0 * M_PI * v);
  return mag * sin(2.0 * M_PI * v);
}

// WebGLFingerprintDetector implementation
// static
bool WebGLFingerprintDetector::DetectFingerprintingAttempt(
    blink::WebGLRenderingContextBase* context,
    const std::string& operation) {
  if (!context) {
    return false;
  }
  
  RecordWebGLOperation(context, operation, "");
  return AnalyzeUsagePattern(context);
}

// static
void WebGLFingerprintDetector::RecordWebGLOperation(
    blink::WebGLRenderingContextBase* context,
    const std::string& operation,
    const std::string& parameters) {
  if (!context) {
    return;
  }
  
  base::AutoLock auto_lock(stats_lock_);
  
  std::string context_id = base::StringPrintf("webgl_%p", context);
  WebGLUsageStats& stats = webgl_stats_[context_id];
  
  int64_t current_time = base::Time::Now().ToInternalValue();
  if (stats.first_operation_time == 0) {
    stats.first_operation_time = current_time;
  }
  stats.last_operation_time = current_time;
  
  stats.operation_sequence.push_back(operation);
  
  // Limit sequence length
  if (stats.operation_sequence.size() > 100) {
    stats.operation_sequence.erase(stats.operation_sequence.begin());
  }
  
  // Categorize operations
  if (operation == "getParameter") {
    stats.parameter_queries++;
    stats.queried_parameters.push_back(parameters);
  } else if (operation == "getSupportedExtensions") {
    stats.extension_queries++;
  } else if (operation == "getShaderPrecisionFormat") {
    stats.shader_queries++;
  } else if (operation.find("Buffer") != std::string::npos) {
    stats.buffer_operations++;
  } else if (operation.find("Texture") != std::string::npos) {
    stats.texture_operations++;
  } else if (operation == "drawArrays" || operation == "drawElements") {
    stats.rendering_operations++;
  }
}

// static
bool WebGLFingerprintDetector::AnalyzeUsagePattern(
    blink::WebGLRenderingContextBase* context) {
  if (!context) {
    return false;
  }
  
  base::AutoLock auto_lock(stats_lock_);
  
  std::string context_id = base::StringPrintf("webgl_%p", context);
  auto it = webgl_stats_.find(context_id);
  if (it == webgl_stats_.end()) {
    return false;
  }
  
  const WebGLUsageStats& stats = it->second;
  return IsLikelyFingerprintingPattern(stats);
}

// static
bool WebGLFingerprintDetector::IsLikelyFingerprintingPattern(
    const WebGLUsageStats& stats) {
  // Check for high query to render ratio
  if (HasHighQueryToRenderRatio(stats)) {
    return true;
  }
  
  // Check for suspicious parameter queries
  if (HasSuspiciousParameterQueries(stats.queried_parameters)) {
    return true;
  }
  
  // Check for fingerprinting operation sequence
  if (HasFingerprintingSequence(stats.operation_sequence)) {
    return true;
  }
  
  return false;
}

// static
bool WebGLFingerprintDetector::HasSuspiciousParameterQueries(
    const std::vector<std::string>& parameters) {
  // Common fingerprinting parameters
  std::vector<std::string> fingerprinting_params = {
    "0x1F00", // GL_VENDOR
    "0x1F01", // GL_RENDERER
    "0x1F02", // GL_VERSION
    "0x8B8C"  // GL_SHADING_LANGUAGE_VERSION
  };
  
  int fingerprinting_queries = 0;
  for (const auto& param : parameters) {
    if (std::find(fingerprinting_params.begin(), fingerprinting_params.end(), param) 
        != fingerprinting_params.end()) {
      fingerprinting_queries++;
    }
  }
  
  return fingerprinting_queries >= 3; // Querying 3+ fingerprinting parameters
}

// static
bool WebGLFingerprintDetector::HasHighQueryToRenderRatio(
    const WebGLUsageStats& stats) {
  if (stats.rendering_operations == 0) {
    return (stats.parameter_queries + stats.extension_queries) > 5;
  }
  
  int total_queries = stats.parameter_queries + stats.extension_queries + stats.shader_queries;
  double ratio = static_cast<double>(total_queries) / stats.rendering_operations;
  
  return ratio > 10.0; // More than 10 queries per render operation
}

// static
bool WebGLFingerprintDetector::HasFingerprintingSequence(
    const std::vector<std::string>& sequence) {
  if (sequence.size() < 5) {
    return false;
  }
  
  // Look for patterns of consecutive parameter queries without rendering
  int consecutive_queries = 0;
  for (const auto& op : sequence) {
    if (op == "getParameter" || op == "getSupportedExtensions" || 
        op == "getShaderPrecisionFormat") {
      consecutive_queries++;
    } else if (op == "drawArrays" || op == "drawElements") {
      consecutive_queries = 0;
    }
    
    if (consecutive_queries > 5) {
      return true;
    }
  }
  
  return false;
}

// WebGLExtensionManager implementation
const std::vector<std::string> WebGLExtensionManager::kFingerprintingExtensions = {
  "WEBGL_debug_renderer_info",
  "WEBGL_debug_shaders"
};

const std::vector<std::string> WebGLExtensionManager::kDefaultExtensions = {
  "ANGLE_instanced_arrays",
  "EXT_blend_minmax",
  "EXT_frag_depth",
  "EXT_shader_texture_lod",
  "EXT_texture_filter_anisotropic",
  "EXT_sRGB",
  "OES_element_index_uint",
  "OES_standard_derivatives",
  "OES_texture_float",
  "OES_texture_half_float",
  "OES_vertex_array_object",
  "WEBGL_color_buffer_float",
  "WEBGL_compressed_texture_s3tc",
  "WEBGL_depth_texture",
  "WEBGL_draw_buffers",
  "WEBGL_lose_context"
};

const std::vector<std::string> WebGLExtensionManager::kDangerousExtensions = {
  "WEBGL_debug_renderer_info",
  "WEBGL_debug_shaders",
  "EXT_disjoint_timer_query"
};

// static
std::vector<std::string> WebGLExtensionManager::GetSupportedExtensions(
    blink::WebGLRenderingContextBase* context) {
  if (!context) {
    return kDefaultExtensions;
  }
  
  WebGLConfig config = WebGLFingerprintProtection::GetConfigForContext(context);
  return FilterExtensions(kDefaultExtensions, config);
}

// static
std::vector<std::string> WebGLExtensionManager::FilterExtensions(
    const std::vector<std::string>& original_extensions,
    const WebGLConfig& config) {
  std::vector<std::string> filtered_extensions;
  
  for (const auto& extension : original_extensions) {
    if (!ShouldHideExtension(extension, config)) {
      filtered_extensions.push_back(extension);
    }
  }
  
  // Add configured extensions
  for (const auto& extension : config.extensions) {
    if (std::find(filtered_extensions.begin(), filtered_extensions.end(), extension) 
        == filtered_extensions.end()) {
      filtered_extensions.push_back(extension);
    }
  }
  
  return filtered_extensions;
}

// static
bool WebGLExtensionManager::ShouldHideExtension(
    const std::string& extension_name,
    const WebGLConfig& config) {
  // Hide dangerous extensions that expose hardware info
  if (std::find(kDangerousExtensions.begin(), kDangerousExtensions.end(), extension_name) 
      != kDangerousExtensions.end()) {
    return true;
  }
  
  // Hide fingerprinting extensions
  if (std::find(kFingerprintingExtensions.begin(), kFingerprintingExtensions.end(), extension_name) 
      != kFingerprintingExtensions.end()) {
    return true;
  }
  
  return false;
}

}  // namespace novebrowse
