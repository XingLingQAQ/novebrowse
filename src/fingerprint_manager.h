#ifndef NOVEBROWSE_FINGERPRINT_MANAGER_H_
#define NOVEBROWSE_FINGERPRINT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/render_frame_host.h"
#include "novebrowse/fingerprint_config.h"

namespace novebrowse {

// 指纹管理器 - 负责管理和应用指纹配置
class FingerprintManager {
 public:
  static FingerprintManager* GetInstance();
  
  // 启用/禁用指纹保护
  static bool IsEnabled();
  static void SetEnabled(bool enabled);
  
  // 配置管理
  bool LoadConfig(const std::string& config_path);
  bool SaveConfig(const std::string& config_path);
  void UpdateConfig(const FingerprintConfig& config);
  
  // 获取指定Frame的指纹配置
  FingerprintConfig GetConfigForFrame(content::RenderFrameHost* frame);
  
  // 设置指定Frame的指纹配置
  void SetConfigForFrame(content::RenderFrameHost* frame, 
                        const FingerprintConfig& config);
  
  // 移除Frame配置
  void RemoveFrameConfig(content::RenderFrameHost* frame);
  
  // 获取默认配置
  const FingerprintConfig& GetDefaultConfig() const { return default_config_; }
  void SetDefaultConfig(const FingerprintConfig& config);
  
  // 设备配置文件管理
  bool LoadDeviceProfiles(const std::string& profiles_path);
  std::vector<std::string> GetAvailableProfiles() const;
  DeviceProfile GetDeviceProfile(const std::string& profile_name) const;
  
  // 行为模式管理
  bool LoadBehaviorPatterns(const std::string& patterns_path);
  std::vector<std::string> GetAvailablePatterns() const;
  BehaviorPattern GetBehaviorPattern(const std::string& pattern_name) const;
  
  // 统计信息
  struct Statistics {
    int total_frames_protected = 0;
    int canvas_operations_spoofed = 0;
    int webgl_parameters_spoofed = 0;
    int navigator_properties_spoofed = 0;
    int webdriver_detections_blocked = 0;
    int audio_contexts_protected = 0;
    int font_enumerations_spoofed = 0;
    int geolocation_requests_spoofed = 0;
    int webrtc_connections_protected = 0;
  };
  
  Statistics GetStatistics() const;
  void ResetStatistics();
  void IncrementStat(const std::string& stat_name);
  
 private:
  friend struct base::DefaultSingletonTraits<FingerprintManager>;
  
  FingerprintManager();
  ~FingerprintManager();
  
  // 禁用拷贝构造和赋值
  FingerprintManager(const FingerprintManager&) = delete;
  FingerprintManager& operator=(const FingerprintManager&) = delete;
  
  // 内部方法
  void InitializeDefaultConfig();
  std::string GenerateFrameId(content::RenderFrameHost* frame);
  
  // 成员变量
  mutable base::Lock lock_;
  static bool enabled_;
  
  FingerprintConfig default_config_;
  std::unordered_map<std::string, FingerprintConfig> frame_configs_;
  std::unordered_map<std::string, DeviceProfile> device_profiles_;
  std::unordered_map<std::string, BehaviorPattern> behavior_patterns_;
  
  mutable Statistics statistics_;
};

// 便捷宏定义
#define FINGERPRINT_MANAGER() FingerprintManager::GetInstance()
#define IS_FINGERPRINT_ENABLED() FingerprintManager::IsEnabled()
#define INCREMENT_FINGERPRINT_STAT(stat) \
  if (IS_FINGERPRINT_ENABLED()) FINGERPRINT_MANAGER()->IncrementStat(stat)

}  // namespace novebrowse

#endif  // NOVEBROWSE_FINGERPRINT_MANAGER_H_
