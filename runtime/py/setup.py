from setuptools import setup

# 与 CMakeLists.txt 中的 RPC_VERSION_MAJOR / RPC_VERSION_MINOR 保持一致。
# 本文件不经过 CMake 处理，因此版本号写字面量，不使用 @...@ 占位符。
setup(
    name='rpc',
    version='1.0',
    packages=['rpc'],
)
