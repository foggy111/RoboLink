#!/usr/bin/env python3
"""按 room_spec.py 写出栅格地图和 Gazebo 世界。

不要手改 maps/ 和 worlds/ 里的生成结果。改几何就改 room_spec.py，再运行：

    python3 rl_nav/scripts/gen_room_assets.py
"""

import argparse
from pathlib import Path

import room_spec


def render_pgm():
    size = room_spec.map_pixels()
    pixels = bytearray()
    for image_row in range(size):
        grid_row = size - 1 - image_row
        for col in range(size):
            pixels.append(0 if room_spec.cell_occupied(col, grid_row) else 254)
    header = f"P5\n{size} {size}\n255\n".encode("ascii")
    return header + bytes(pixels)


def render_yaml():
    origin = -room_spec.HALF_EXTENT
    return (
        "image: room.pgm\n"
        "mode: trinary\n"
        f"resolution: {room_spec.RESOLUTION:.2f}\n"
        f"origin: [{origin:.1f}, {origin:.1f}, 0.0]\n"
        "negate: 0\n"
        "occupied_thresh: 0.65\n"
        "free_thresh: 0.25\n"
    )


def _box(name, x, y, size_x, size_y, red, green, blue):
    z = room_spec.WALL_HEIGHT / 2.0
    height = room_spec.WALL_HEIGHT
    return (
        f'    <model name="{name}">\n'
        "      <static>true</static>\n"
        f"      <pose>{x:.4f} {y:.4f} {z:.4f} 0 0 0</pose>\n"
        "      <link name=\"link\">\n"
        "        <collision name=\"collision\">\n"
        "          <geometry>\n"
        f"            <box><size>{size_x:.4f} {size_y:.4f} {height:.4f}</size></box>\n"
        "          </geometry>\n"
        "        </collision>\n"
        "        <visual name=\"visual\">\n"
        "          <geometry>\n"
        f"            <box><size>{size_x:.4f} {size_y:.4f} {height:.4f}</size></box>\n"
        "          </geometry>\n"
        "          <material>\n"
        f"            <ambient>{red:.3f} {green:.3f} {blue:.3f} 1</ambient>\n"
        f"            <diffuse>{red:.3f} {green:.3f} {blue:.3f} 1</diffuse>\n"
        "          </material>\n"
        "        </visual>\n"
        "      </link>\n"
        "    </model>\n"
    )


def render_world():
    half = room_spec.HALF_EXTENT
    thick = room_spec.WALL_THICKNESS
    span = 2.0 * half
    center_x, center_y = room_spec.OBSTACLE_CENTER
    size_x, size_y = room_spec.OBSTACLE_SIZE
    models = [
        _box("wall_west", -half + thick / 2.0, 0.0, thick, span, 0.70, 0.70, 0.75),
        _box("wall_east", half - thick / 2.0, 0.0, thick, span, 0.70, 0.70, 0.75),
        _box("wall_south", 0.0, -half + thick / 2.0, span, thick, 0.70, 0.70, 0.75),
        _box("wall_north", 0.0, half - thick / 2.0, span, thick, 0.70, 0.70, 0.75),
        _box("obstacle", center_x, center_y, size_x, size_y, 0.80, 0.40, 0.20),
    ]
    body = "".join(models)
    return (
        "<?xml version=\"1.0\"?>\n"
        "<!-- 由 rl_nav/scripts/gen_room_assets.py 生成。改 room_spec.py 后重新生成。 -->\n"
        "<sdf version=\"1.6\">\n"
        "  <world name=\"rl_room\">\n"
        "    <include><uri>model://sun</uri></include>\n"
        "    <include><uri>model://ground_plane</uri></include>\n"
        "    <physics type=\"ode\">\n"
        "      <max_step_size>0.001</max_step_size>\n"
        "      <real_time_update_rate>1000</real_time_update_rate>\n"
        "    </physics>\n"
        f"{body}"
        "  </world>\n"
        "</sdf>\n"
    )


def generate(nav_root):
    nav_root = Path(nav_root)
    maps = nav_root / "maps"
    worlds = nav_root / "worlds"
    maps.mkdir(parents=True, exist_ok=True)
    worlds.mkdir(parents=True, exist_ok=True)
    (maps / "room.pgm").write_bytes(render_pgm())
    (maps / "room.yaml").write_text(render_yaml(), encoding="utf-8")
    (worlds / "room.world").write_text(render_world(), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--nav-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="rl_nav 目录，默认是本脚本的上一级",
    )
    args = parser.parse_args()
    generate(args.nav_root)


if __name__ == "__main__":
    main()
