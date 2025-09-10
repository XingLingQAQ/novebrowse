#ifndef NOVEBROWSE_FINGERPRINT_CONFIG_H_
#define NOVEBROWSE_FINGERPRINT_CONFIG_H_

#include <string>
#include <vector>
#include <unordered_map>
#include "base/values.h"
#include "novebrowse/mojom/fingerprint.mojom.h"

namespace novebrowse {

// Canvas指纹保护配置
struct CanvasConfig {
  bool enabled = true;
  bool add_noise = true;
  double noise_level = 0.1;  // 0.0-1.0
  bool spoof_text_metrics = true;
  bool protect_data_url = true;
  bool protect_image_data = true;
};

// WebGL指纹保护配置
struct WebGLConfig {
  bool enabled = true;
  std::string vendor = "Intel Inc.";
  std::string renderer = "Intel(R) HD Graphics 620";
  std::string version = "OpenGL ES 2.0 (ANGLE 2.1.0.0)";
  std::string shading_language_version = "OpenGL ES GLSL ES 1.00 (ANGLE 2.1.0.0)";
  std::vector<std::string> extensions;
  std::unordered_map<std::string, std::string> parameters;
  bool add_noise_to_buffers = true;
  double buffer_noise_level = 0.01;
};

// Navigator对象保护配置
struct NavigatorConfig {
  bool enabled = true;
  std::string user_agent;
  std::string platform;
  std::vector<std::string> languages;
  int hardware_concurrency = 4;
  uint64_t device_memory = 8;  // GB
  bool hide_webdriver = true;
  bool spoof_plugins = true;
  std::vector<std::string> mime_types;
};

// 音频指纹保护配置
struct AudioConfig {
  bool enabled = true;
  bool add_noise = true;
  double noise_level = 0.001;
  bool protect_analyser_node = true;
  bool protect_offline_context = true;
  int sample_rate = 44100;
  int buffer_size = 4096;
};

// 字体指纹保护配置
struct FontConfig {
  bool enabled = true;
  bool spoof_enumeration = true;
  bool spoof_metrics = true;
  std::vector<std::string> available_fonts;
  std::unordered_map<std::string, double> font_metrics_offsets;
};

// WebRTC保护配置
struct WebRTCConfig {
  bool enabled = true;
  bool mask_local_ips = true;
  bool disable_webrtc = false;
  std::string fake_public_ip = "8.8.8.8";
  std::vector<std::string> allowed_ice_servers;
  bool block_device_enumeration = true;
};

// 地理位置保护配置
struct GeolocationConfig {
  bool enabled = true;
  bool spoof_location = true;
  double latitude = 40.7128;   // 纽约
  double longitude = -74.0060;
  double accuracy = 10.0;
  bool block_high_accuracy = true;
};

// 屏幕指纹保护配置
struct ScreenConfig {
  bool enabled = true;
  int width = 1920;
  int height = 1080;
  int color_depth = 24;
  int pixel_depth = 24;
  double device_pixel_ratio = 1.0;
  std::string orientation = "landscape-primary";
};

// 时区保护配置
struct TimezoneConfig {
  bool enabled = true;
  std::string timezone = "America/New_York";
  int timezone_offset = -300;  // 分钟
  bool spoof_date_methods = true;
};

// 设备配置文件
struct DeviceProfile {
  std::string name;
  std::string description;
  NavigatorConfig navigator;
  ScreenConfig screen;
  CanvasConfig canvas;
  WebGLConfig webgl;
  AudioConfig audio;
  FontConfig font;
  std::unordered_map<std::string, std::string> custom_properties;
};

// 行为模式配置
struct BehaviorPattern {
  std::string name;
  std::string description;
  
  // 鼠标行为
  struct MouseBehavior {
    double movement_speed = 1.0;
    double click_delay_ms = 100.0;
    bool add_random_movements = true;
    double random_movement_probability = 0.1;
  } mouse;
  
  // 键盘行为
  struct KeyboardBehavior {
    double typing_speed_wpm = 60.0;
    double key_press_delay_ms = 50.0;
    bool add_typing_errors = true;
    double error_probability = 0.02;
  } keyboard;
  
  // 滚动行为
  struct ScrollBehavior {
    double scroll_speed = 1.0;
    bool smooth_scrolling = true;
    double pause_probability = 0.3;
    int pause_duration_ms = 500;
  } scroll;
  
  // 页面交互行为
  struct InteractionBehavior {
    double page_dwell_time_ms = 5000.0;
    bool simulate_reading = true;
    double link_click_probability = 0.8;
    double form_fill_speed = 1.0;
  } interaction;
};

// 反检测配置
struct AntiDetectionConfig {
  bool enabled = true;
  
  // WebDriver检测阻止
  struct WebDriverProtection {
    bool hide_webdriver_property = true;
    bool hide_automation_flags = true;
    bool spoof_chrome_runtime = true;
    bool hide_selenium_variables = true;
    std::vector<std::string> blocked_properties;
  } webdriver;
  
  // 自动化检测阻止
  struct AutomationProtection {
    bool hide_headless_flags = true;
    bool spoof_user_interaction = true;
    bool add_human_delays = true;
    bool randomize_request_timing = true;
    int min_delay_ms = 100;
    int max_delay_ms = 2000;
  } automation;
  
  // JavaScript注入保护
  struct JSInjectionProtection {
    bool detect_puppeteer = true;
    bool detect_playwright = true;
    bool detect_selenium = true;
    bool block_detection_scripts = true;
    std::vector<std::string> blocked_script_patterns;
  } js_injection;
};

// 主指纹配置结构
struct FingerprintConfig {
  bool enabled = true;
  std::string profile_name = "default";
  std::string device_profile = "windows_chrome";
  std::string behavior_pattern = "normal_user";
  
  CanvasConfig canvas;
  WebGLConfig webgl;
  NavigatorConfig navigator;
  AudioConfig audio;
  FontConfig font;
  WebRTCConfig webrtc;
  GeolocationConfig geolocation;
  ScreenConfig screen;
  TimezoneConfig timezone;
  AntiDetectionConfig anti_detection;
  
  // 自定义JavaScript注入
  std::vector<std::string> custom_js_injections;
  
  // 配置元数据
  std::string created_at;
  std::string updated_at;
  std::string version = "1.0.0";
  
  // 转换为Mojo结构
  mojom::FingerprintConfigPtr ToMojoStruct() const;
  
  // 从Mojo结构创建
  static FingerprintConfig FromMojoStruct(const mojom::FingerprintConfigPtr& mojo_config);
  
  // JSON序列化
  base::Value ToValue() const;
  static FingerprintConfig FromValue(const base::Value& value);
  
  // 验证配置
  bool IsValid() const;
  std::vector<std::string> GetValidationErrors() const;
  
  // 合并配置
  void MergeWith(const FingerprintConfig& other);
  
  // 生成配置哈希
  std::string GetConfigHash() const;
};

}  // namespace novebrowse

#endif  // NOVEBROWSE_FINGERPRINT_CONFIG_H_
