FROM ros:humble

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      pkg-config \
      protobuf-compiler \
      libprotobuf-dev \
      python3-colcon-common-extensions \
      ros-humble-rclcpp-lifecycle \
      ros-humble-geometry-msgs \
      ros-humble-nav-msgs \
      ca-certificates \
      curl \
      unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/robolink
COPY . /opt/robolink

# 纯 CMake，不依赖 colcon。失败会让镜像构建停下来。
RUN cmake -S rl_transport -B /tmp/build-rl_transport -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/build-rl_transport --parallel \
    && ctest --test-dir /tmp/build-rl_transport --output-on-failure \
    && cmake -S rl_hal -B /tmp/build-rl_hal -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/build-rl_hal --parallel \
    && ctest --test-dir /tmp/build-rl_hal --output-on-failure

WORKDIR /ws
RUN mkdir -p /ws/src \
    && cp -a /opt/robolink /ws/src/RoboLink \
    && bash -c 'set -eo pipefail; \
        source /opt/ros/humble/setup.bash; \
        cd /ws; \
        colcon build --base-paths src/RoboLink --packages-select rl_transport rl_hal rl_bridge; \
        colcon test --base-paths src/RoboLink --packages-select rl_transport rl_hal --event-handlers console_direct+; \
        colcon test-result --verbose'

CMD ["bash", "-lc", "source /opt/ros/humble/setup.bash && source /ws/install/setup.bash && exec bash"]
