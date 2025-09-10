#ifndef NOVEBROWSE_WEBGL_FINGERPRINT_PROTECTION_H_
#define NOVEBROWSE_WEBGL_FINGERPRINT_PROTECTION_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "novebrowse/fingerprint_config.h"

namespace novebrowse {

// WebGL指纹保护实现类
class WebGLFingerprintProtection {
 public:
  // 检查是否启用WebGL保护
  static bool IsEnabled();
  
  // 获取伪造的WebGL参数
  static std::optional<v8::Local<v8::Value>> GetSpoofedParameter(
      GLenum pname,
      blink::WebGLRenderingContextBase* context);
  
  // 获取伪造的字符串参数
  static WTF::String GetSpoofedStringParameter(
      GLenum pname,
      blink::WebGLRenderingContextBase* context);
  
  // 处理WebGL缓冲区数据
  static void ProcessBufferData(
      void* buffer_data,
      size_t data_size,
      const WebGLConfig& config);
  
  // 获取伪造的扩展列表
  static std::vector<std::string> GetSpoofedExtensions(
      blink::WebGLRenderingContextBase* context);
  
  // 获取伪造的着色器精度格式
  static void GetSpoofedShaderPrecisionFormat(
      GLenum shadertype,
      GLenum precisiontype,
      GLint* range,
      GLint* precision);
  
  // 处理WebGL纹理数据
  static void ProcessTextureData(
      const void* pixels,
      GLenum format,
      GLenum type,
      GLsizei width,
      GLsizei height,
      const WebGLConfig& config);
  
  // 获取WebGL配置
  static WebGLConfig GetConfigForContext(
      blink::WebGLRenderingContextBase* context);
  
 private:
  // WebGL参数映射
  static const std::unordered_map<GLenum, std::string> kParameterNames;
  static const std::unordered_map<GLenum, std::string> kDefaultStringValues;
  static const std::unordered_map<GLenum, std::vector<GLint>> kDefaultIntArrayValues;
  static const std::unordered_map<GLenum, std::vector<GLfloat>> kDefaultFloatArrayValues;
  
  // 内部辅助函数
  static std::string GetContextId(blink::WebGLRenderingContextBase* context);
  static uint32_t GenerateNoiseSeed(blink::WebGLRenderingContextBase* context);
  static void ApplyBufferNoise(void* buffer, size_t size, uint32_t seed, double noise_level);
  
  // 参数伪造函数
  static v8::Local<v8::Value> CreateSpoofedVendor(v8::Isolate* isolate, const WebGLConfig& config);
  static v8::Local<v8::Value> CreateSpoofedRenderer(v8::Isolate* isolate, const WebGLConfig& config);
  static v8::Local<v8::Value> CreateSpoofedVersion(v8::Isolate* isolate, const WebGLConfig& config);
  static v8::Local<v8::Value> CreateSpoofedShadingLanguageVersion(v8::Isolate* isolate, const WebGLConfig& config);
  static v8::Local<v8::Value> CreateSpoofedExtensions(v8::Isolate* isolate, const WebGLConfig& config);
  static v8::Local<v8::Value> CreateSpoofedParameter(v8::Isolate* isolate, GLenum pname, const WebGLConfig& config);
};

// WebGL噪声生成器
class WebGLNoiseGenerator {
 public:
  explicit WebGLNoiseGenerator(uint32_t seed);
  
  // 生成缓冲区噪声
  void GenerateBufferNoise(void* buffer, size_t size, double noise_level);
  
  // 生成浮点噪声
  float GenerateFloatNoise(float original_value, double noise_level);
  
  // 生成整数噪声
  int GenerateIntNoise(int original_value, double noise_level);
  
  // 重置种子
  void SetSeed(uint32_t seed);
  
 private:
  uint32_t seed_;
  
  // 伪随机数生成
  uint32_t NextRandom();
  float NextFloat();
  double NextGaussian();
};

// WebGL指纹检测器
class WebGLFingerprintDetector {
 public:
  // 检测WebGL指纹提取尝试
  static bool DetectFingerprintingAttempt(
      blink::WebGLRenderingContextBase* context,
      const std::string& operation);
  
  // 记录WebGL操作
  static void RecordWebGLOperation(
      blink::WebGLRenderingContextBase* context,
      const std::string& operation,
      const std::string& parameters);
  
  // 分析WebGL使用模式
  static bool AnalyzeUsagePattern(
      blink::WebGLRenderingContextBase* context);
  
 private:
  struct WebGLUsageStats {
    int parameter_queries = 0;
    int extension_queries = 0;
    int shader_queries = 0;
    int buffer_operations = 0;
    int texture_operations = 0;
    int rendering_operations = 0;
    std::vector<std::string> queried_parameters;
    std::vector<std::string> operation_sequence;
    int64_t first_operation_time = 0;
    int64_t last_operation_time = 0;
  };
  
  static std::unordered_map<std::string, WebGLUsageStats> webgl_stats_;
  static base::Lock stats_lock_;
  
  // 指纹识别模式检测
  static bool IsLikelyFingerprintingPattern(const WebGLUsageStats& stats);
  static bool HasSuspiciousParameterQueries(const std::vector<std::string>& parameters);
  static bool HasHighQueryToRenderRatio(const WebGLUsageStats& stats);
  static bool HasFingerprintingSequence(const std::vector<std::string>& sequence);
};

// WebGL扩展管理器
class WebGLExtensionManager {
 public:
  // 获取支持的扩展列表
  static std::vector<std::string> GetSupportedExtensions(
      blink::WebGLRenderingContextBase* context);
  
  // 过滤扩展列表
  static std::vector<std::string> FilterExtensions(
      const std::vector<std::string>& original_extensions,
      const WebGLConfig& config);
  
  // 检查扩展是否应该隐藏
  static bool ShouldHideExtension(
      const std::string& extension_name,
      const WebGLConfig& config);
  
 private:
  // 常见的指纹识别相关扩展
  static const std::vector<std::string> kFingerprintingExtensions;
  
  // 默认扩展集合
  static const std::vector<std::string> kDefaultExtensions;
  
  // 危险扩展（可能暴露硬件信息）
  static const std::vector<std::string> kDangerousExtensions;
};

}  // namespace novebrowse

#endif  // NOVEBROWSE_WEBGL_FINGERPRINT_PROTECTION_H_
