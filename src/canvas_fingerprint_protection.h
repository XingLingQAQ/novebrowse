#ifndef NOVEBROWSE_CANVAS_FINGERPRINT_PROTECTION_H_
#define NOVEBROWSE_CANVAS_FINGERPRINT_PROTECTION_H_

#include <memory>
#include <string>
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "novebrowse/fingerprint_config.h"

namespace novebrowse {

// Canvas指纹保护实现类
class CanvasFingerprintProtection {
 public:
  // 检查是否启用Canvas保护
  static bool IsEnabled();
  
  // 处理ImageData - 添加噪声保护
  static blink::ImageData* ProcessImageData(
      blink::ImageData* original_data,
      blink::CanvasRenderingContextHost* host);
  
  // 处理Canvas数据URL - 添加噪声保护
  static WTF::String ProcessDataURL(
      const WTF::String& original_url,
      blink::CanvasRenderingContextHost* host);
  
  // 处理文本度量 - 添加轻微偏移
  static blink::TextMetrics* ProcessTextMetrics(
      blink::TextMetrics* original_metrics,
      blink::CanvasRenderingContextHost* host);
  
  // 处理Canvas像素数据
  static void ProcessPixelData(
      uint8_t* pixel_data,
      size_t data_length,
      const CanvasConfig& config);
  
  // 添加Canvas噪声
  static void AddCanvasNoise(
      SkBitmap& bitmap,
      double noise_level);
  
  // 生成确定性噪声种子
  static uint32_t GenerateNoiseSeed(
      blink::CanvasRenderingContextHost* host);
  
  // 获取Canvas配置
  static CanvasConfig GetConfigForHost(
      blink::CanvasRenderingContextHost* host);
  
 private:
  // 内部噪声生成函数
  static double GenerateNoise(uint32_t seed, int x, int y, int channel);
  
  // 应用像素级噪声
  static void ApplyPixelNoise(
      uint8_t* pixel,
      uint32_t seed,
      int x,
      int y,
      double noise_level);
  
  // 文本度量偏移
  static void ApplyTextMetricsOffset(
      blink::TextMetrics* metrics,
      const CanvasConfig& config);
  
  // 计算Canvas指纹哈希
  static std::string CalculateCanvasHash(
      blink::CanvasRenderingContextHost* host);
};

// Canvas噪声生成器
class CanvasNoiseGenerator {
 public:
  explicit CanvasNoiseGenerator(uint32_t seed);
  
  // 生成像素噪声
  uint8_t GeneratePixelNoise(int x, int y, int channel, uint8_t original_value, double noise_level);
  
  // 生成浮点噪声
  double GenerateFloatNoise(int x, int y, double noise_level);
  
  // 重置种子
  void SetSeed(uint32_t seed);
  
 private:
  uint32_t seed_;
  
  // 伪随机数生成
  uint32_t NextRandom();
  double NextFloat();
  
  // 噪声函数
  double PerlinNoise(double x, double y);
  double Fade(double t);
  double Lerp(double t, double a, double b);
  double Grad(int hash, double x, double y);
};

// Canvas指纹检测器
class CanvasFingerprintDetector {
 public:
  // 检测Canvas指纹提取尝试
  static bool DetectFingerprintingAttempt(
      blink::CanvasRenderingContextHost* host,
      const std::string& operation);
  
  // 记录Canvas操作
  static void RecordCanvasOperation(
      blink::CanvasRenderingContextHost* host,
      const std::string& operation,
      const std::string& parameters);
  
  // 分析Canvas使用模式
  static bool AnalyzeUsagePattern(
      blink::CanvasRenderingContextHost* host);
  
 private:
  struct CanvasUsageStats {
    int draw_operations = 0;
    int read_operations = 0;
    int text_operations = 0;
    int image_data_reads = 0;
    int data_url_exports = 0;
    std::vector<std::string> operation_sequence;
    int64_t first_operation_time = 0;
    int64_t last_operation_time = 0;
  };
  
  static std::unordered_map<std::string, CanvasUsageStats> canvas_stats_;
  static base::Lock stats_lock_;
  
  // 指纹识别模式
  static bool IsLikelyFingerprintingPattern(const CanvasUsageStats& stats);
  static bool HasSuspiciousOperationSequence(const std::vector<std::string>& sequence);
  static bool HasHighReadToWriteRatio(const CanvasUsageStats& stats);
};

}  // namespace novebrowse

#endif  // NOVEBROWSE_CANVAS_FINGERPRINT_PROTECTION_H_
