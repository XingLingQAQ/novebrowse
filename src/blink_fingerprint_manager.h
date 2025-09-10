#ifndef NOVEBROWSE_BLINK_FINGERPRINT_MANAGER_H_
#define NOVEBROWSE_BLINK_FINGERPRINT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "novebrowse/fingerprint_config.h"

namespace novebrowse {

// Blink层指纹管理器 - 负责在渲染器进程中管理指纹伪造
class BlinkFingerprintManager final 
    : public blink::GarbageCollected<BlinkFingerprintManager>,
      public blink::Supplement<blink::LocalFrame> {
 public:
  static const char kSupplementName[];
  
  // 获取Frame对应的指纹管理器
  static BlinkFingerprintManager* FromFrame(blink::LocalFrame* frame);
  
  // 创建指纹管理器
  static BlinkFingerprintManager* Create(blink::LocalFrame* frame);
  
  explicit BlinkFingerprintManager(blink::LocalFrame& frame);
  ~BlinkFingerprintManager() override = default;
  
  // 更新指纹配置
  void UpdateConfig(const FingerprintConfig& config);
  
  // Navigator属性伪造
  WTF::String GetSpoofedUserAgent() const;
  WTF::String GetSpoofedPlatform() const;
  WTF::Vector<WTF::String> GetSpoofedLanguages() const;
  int GetSpoofedHardwareConcurrency() const;
  uint64_t GetSpoofedDeviceMemory() const;
  bool ShouldHideWebDriver() const;
  
  // Screen属性伪造
  int GetSpoofedScreenWidth() const;
  int GetSpoofedScreenHeight() const;
  int GetSpoofedScreenColorDepth() const;
  int GetSpoofedScreenPixelDepth() const;
  double GetSpoofedDevicePixelRatio() const;
  
  // 时区伪造
  WTF::String GetSpoofedTimezone() const;
  int GetSpoofedTimezoneOffset() const;
  
  // 地理位置伪造
  bool ShouldSpoofGeolocation() const;
  double GetSpoofedLatitude() const;
  double GetSpoofedLongitude() const;
  double GetSpoofedAccuracy() const;
  
  // Canvas保护
  bool ShouldProtectCanvas() const;
  double GetCanvasNoiseLevel() const;
  bool ShouldSpoofTextMetrics() const;
  
  // WebGL保护
  bool ShouldProtectWebGL() const;
  WTF::String GetSpoofedWebGLVendor() const;
  WTF::String GetSpoofedWebGLRenderer() const;
  WTF::String GetSpoofedWebGLVersion() const;
  WTF::Vector<WTF::String> GetSpoofedWebGLExtensions() const;
  
  // 音频保护
  bool ShouldProtectAudio() const;
  double GetAudioNoiseLevel() const;
  int GetSpoofedSampleRate() const;
  
  // 字体保护
  bool ShouldProtectFonts() const;
  WTF::Vector<WTF::String> GetSpoofedAvailableFonts() const;
  bool ShouldSpoofFontMetrics() const;
  
  // WebRTC保护
  bool ShouldProtectWebRTC() const;
  bool ShouldMaskLocalIPs() const;
  WTF::String GetFakePublicIP() const;
  
  // 反检测功能
  bool ShouldHideAutomationFlags() const;
  bool ShouldSpoofChromeRuntime() const;
  bool ShouldBlockDetectionScripts() const;
  WTF::Vector<WTF::String> GetBlockedScriptPatterns() const;
  
  // 配置状态
  bool IsConfigured() const { return configured_; }
  const FingerprintConfig& GetConfig() const { return config_; }
  
  // 统计信息
  void IncrementOperationCount(const WTF::String& operation);
  int GetOperationCount(const WTF::String& operation) const;
  
  // Garbage collection
  void Trace(blink::Visitor* visitor) const override;
  
 private:
  // 初始化默认配置
  void InitializeDefaultConfig();
  
  // 验证配置
  bool ValidateConfig(const FingerprintConfig& config) const;
  
  // 生成确定性值
  uint32_t GenerateSeed() const;
  WTF::String GenerateConsistentValue(const WTF::String& base, uint32_t seed) const;
  
  // 成员变量
  blink::Member<blink::LocalFrame> frame_;
  FingerprintConfig config_;
  bool configured_ = false;
  
  // 操作统计
  mutable std::unordered_map<std::string, int> operation_counts_;
  
  // 缓存的伪造值
  mutable WTF::String cached_user_agent_;
  mutable WTF::String cached_platform_;
  mutable WTF::Vector<WTF::String> cached_languages_;
  mutable WTF::Vector<WTF::String> cached_webgl_extensions_;
  mutable WTF::Vector<WTF::String> cached_fonts_;
  mutable bool cache_valid_ = false;
  
  // 清除缓存
  void ClearCache() const;
};

// JavaScript注入管理器
class JSInjectionManager {
 public:
  // 注入反检测脚本
  static void InjectAntiDetectionScripts(blink::LocalFrame* frame);
  
  // 注入Canvas保护脚本
  static void InjectCanvasProtection(blink::LocalFrame* frame, const CanvasConfig& config);
  
  // 注入WebGL保护脚本
  static void InjectWebGLProtection(blink::LocalFrame* frame, const WebGLConfig& config);
  
  // 注入Navigator伪造脚本
  static void InjectNavigatorSpoofing(blink::LocalFrame* frame, const NavigatorConfig& config);
  
  // 注入WebRTC保护脚本
  static void InjectWebRTCProtection(blink::LocalFrame* frame, const WebRTCConfig& config);
  
  // 注入自定义脚本
  static void InjectCustomScript(blink::LocalFrame* frame, const WTF::String& script);
  
 private:
  // 生成脚本内容
  static WTF::String GenerateAntiDetectionScript();
  static WTF::String GenerateCanvasProtectionScript(const CanvasConfig& config);
  static WTF::String GenerateWebGLProtectionScript(const WebGLConfig& config);
  static WTF::String GenerateNavigatorSpoofingScript(const NavigatorConfig& config);
  static WTF::String GenerateWebRTCProtectionScript(const WebRTCConfig& config);
  
  // 脚本模板
  static const char kAntiDetectionTemplate[];
  static const char kCanvasProtectionTemplate[];
  static const char kWebGLProtectionTemplate[];
  static const char kNavigatorSpoofingTemplate[];
  static const char kWebRTCProtectionTemplate[];
};

}  // namespace novebrowse

#endif  // NOVEBROWSE_BLINK_FINGERPRINT_MANAGER_H_
