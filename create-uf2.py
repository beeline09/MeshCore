#!/usr/bin/python3

# Post-build script: automatically converts firmware.hex to firmware.uf2
# for nRF52 targets using the bundled uf2conv.py tool.

import os

Import("env")

firmware_hex = "${BUILD_DIR}/${PROGNAME}.hex"
uf2_file = os.environ.get("UF2_FILE_PATH", "${BUILD_DIR}/${PROGNAME}.uf2")

def create_uf2_action(source, target, env):
    uf2_cmd = " ".join(
        [
            '"$PYTHONEXE"',
            '"$PROJECT_DIR/bin/uf2conv/uf2conv.py"',
            '-f', '0xADA52840',
            '-c', firmware_hex,
            '-o', uf2_file,
        ]
    )
    env.Execute(uf2_cmd)

env.AddPostAction(firmware_hex, create_uf2_action)
