"""Fetch the pinned minimp3 header before PlatformIO compiles the app."""

Import("env")

import hashlib
import os
from urllib.request import urlopen


URL = "https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h"
SHA256 = "5fb296a790734772b65a5514438cc06d14f24e539c13475ff2a7f737681c82c0"
target_dir = os.path.join(env.subst("$BUILD_DIR"), "generated", "minimp3")
target = os.path.join(target_dir, "minimp3.h")


def valid_header(path):
    if not os.path.isfile(path):
        return False
    with open(path, "rb") as source:
        return hashlib.sha256(source.read()).hexdigest() == SHA256


if not valid_header(target):
    os.makedirs(target_dir, exist_ok=True)
    with urlopen(URL, timeout=30) as response:
        content = response.read()
    if hashlib.sha256(content).hexdigest() != SHA256:
        raise RuntimeError("Downloaded minimp3 header did not match its expected SHA-256")
    with open(target, "wb") as destination:
        destination.write(content)

env.Append(CPPPATH=[target_dir])
