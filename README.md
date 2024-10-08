# LIBQRC

libqrc is a library to communicate between APPs and motor control board.

## Overview

Libqrc used to communicate with Motor control board .

Libqrc, which is a dynamic library, is base on tinyframe protocol.

## Build

Currently, we only support use QCLINUX to build

1. Setup environments follow this document 's [Set up the cross-compile environment.](https://docs.qualcomm.com/bundle/publicresource/topics/80-65220-2/develop-your-first-application_6.html?product=1601111740013072&facet=Qualcomm%20Intelligent%20Robotics%20(QIRP)%20Product%20SDK&state=releasecandidate) part

2. Create `ros_ws` directory in `<qirp_decompressed_workspace>/qirp-sdk/`

3. Clone this repository under `<qirp_decompressed_workspace>/qirp-sdk/ros_ws`
     ```bash
     git clone https://github.com/quic-qrb-ros/robot_base_qrc.git
     ```
4. Build this project
     ```bash
     export AMENT_PREFIX_PATH="${OECORE_TARGET_SYSROOT}/usr;${OECORE_NATIVE_SYSROOT}/usr"
     export PYTHONPATH=${PYTHONPATH}:${OECORE_TARGET_SYSROOT}/usr/lib/python3.10/site-packages

     colcon build --merge-install --cmake-args \
       -DPython3_ROOT_DIR=${OECORE_TARGET_SYSROOT}/usr \
       -DPython3_NumPy_INCLUDE_DIR=${OECORE_TARGET_SYSROOT}/usr/lib/python3.10/site-packages/numpy/core/include \
       -DPYTHON_SOABI=cpython-310-aarch64-linux-gnu -DCMAKE_STAGING_PREFIX=$(pwd)/install \
       -DCMAKE_PREFIX_PATH=$(pwd)/install/share \
       -DBUILD_TESTING=OFF
     ```
5. Push to the device & Install
     ```bash
     cd `<qirp_decompressed_workspace>/qirp-sdk/ros_ws/install`
     tar czvf libqrc.tar.gz lib share
     scp libqrc.tar.gz root@[ip-addr]:/opt/
     ssh root@[ip-addr]
     (ssh) tar -zxf /opt/libqrc.tar.gz -C /opt/qcom/qirp-sdk/usr/
     ```

## Contributions

Thanks for your interest in contributing to libqrc! Please read our [Contributions Page](CONTRIBUTING.md) for more information on contributing features or bug fixes. We look forward to your participation!

## License

Libqrc is licensed under the BSD-3-clause "New" or "Revised" License. 

Check out the [LICENSE](LICENSE) for more details.
