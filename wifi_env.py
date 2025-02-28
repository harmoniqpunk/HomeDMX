import os

# PlatformIO script to handle WiFi credentials from environment variables

# Define the environment variable function
def env_processor(env):
    # Get WiFi credentials from environment variables
    wifi_ssid = os.environ.get("HOMEDMX_WIFI_SSID")
    wifi_pass = os.environ.get("HOMEDMX_WIFI_PASS")

    # Only add flags if the environment variables exist
    if wifi_ssid:
        env.Append(CPPDEFINES=[
            ("HOMEDMX_WIFI_SSID", '\\"%s\\"' % wifi_ssid)
        ])
        print("Using WiFi SSID from environment variable: %s (masked)" % wifi_ssid[:2] + "*" * (len(wifi_ssid) - 2))
    else:
        print("WARNING: HOMEDMX_WIFI_SSID environment variable not found. Using default value.")

    if wifi_pass:
        env.Append(CPPDEFINES=[
            ("HOMEDMX_WIFI_PASS", '\\"%s\\"' % wifi_pass)
        ])
        print("Using WiFi password from environment variable (masked)")
    else:
        print("WARNING: HOMEDMX_WIFI_PASS environment variable not found. Using default value.")

# This is called by PlatformIO automatically and the env is passed as a parameter
# No need to reference it directly here 