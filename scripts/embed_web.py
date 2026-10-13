"""Keep the embedded page current when PlatformIO skips CMake regeneration.

Run before builder setup so changed assembly is seen by SCons. On a clean build,
the existing CMake hook creates the assembly. This hook only refreshes an existing
file and uses the same ESP-IDF converter and symbol names.
"""
from pathlib import Path
import subprocess
import tempfile

Import("env")


def refresh_embedded_page(build_env):
    project = Path(build_env.subst("$PROJECT_DIR"))
    generated = Path(build_env.subst("$BUILD_DIR")) / "index.html.S"
    if not generated.exists():
        return
    source = project / "main" / "transfer" / "web" / "index.html"
    framework = build_env.PioPlatform().get_package_dir("framework-espidf")
    cmake_package = build_env.PioPlatform().get_package_dir("tool-cmake")
    cmake = str(Path(cmake_package) / "bin" / "cmake")
    converter = Path(framework) / "tools" / "cmake" / "scripts" / "data_file_embed_asm.cmake"
    with tempfile.TemporaryDirectory(prefix="esp32-web-embed-") as temporary:
        candidate = Path(temporary) / "index.html.S"
        subprocess.run([
            cmake, f"-DDATA_FILE={source}", f"-DSOURCE_FILE={candidate}",
            "-DFILE_TYPE=BINARY", "-P", str(converter),
        ], check=True)
        content = candidate.read_bytes()
        if generated.read_bytes() != content:
            generated.write_bytes(content)
            print("Updated embedded transfer page: index.html.S")


refresh_embedded_page(env)
