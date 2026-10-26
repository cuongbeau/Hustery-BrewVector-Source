import datetime
import os

Import("env")

# Generate the build version, e.g., 2026.05.12-1430
build_version = datetime.datetime.now().strftime("%Y.%m.%d-%H%M")

print(f"Injecting BUILD_VERSION: {build_version}")

# Add the build version as a macro
env.Append(CPPDEFINES=[
    ("BUILD_VERSION", f'\\"{build_version}\\"')
])

# Arduino ESP32 3's WiFi sources include Network.h, but PlatformIO's library
# finder can omit the sibling Network include path when compiling WiFi itself.
framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if framework_dir:
    network_include = os.path.join(framework_dir, "libraries", "Network", "src")
    if os.path.isdir(network_include):
        env.Append(CPPPATH=[network_include])
