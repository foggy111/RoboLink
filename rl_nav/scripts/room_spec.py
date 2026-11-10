"""房间和默认导航目标的几何常量。

地图、Gazebo 世界、资产测试都从这里取数。URDF 里的轮距和轮半径
要和这里一致，由 rl_nav/test/test_assets.py 对一下。

这些是模型尺寸和演示用的目标点，不是测量结果。
"""

RESOLUTION = 0.05
HALF_EXTENT = 5.0
WALL_THICKNESS = 0.15
WALL_HEIGHT = 0.6

# 方柱中心 (x, y)，边长 (x, y)。放在出生点东南方，不挡默认目标。
OBSTACLE_CENTER = (1.5, -1.5)
OBSTACLE_SIZE = (0.6, 0.6)

# 地图坐标。Gazebo 里还会把模型抬高 SPAWN_Z，避免轮子生成时嵌进地面。
SPAWN_X = 0.0
SPAWN_Y = 0.0
SPAWN_YAW = 0.0
SPAWN_Z = 0.01

GOAL_X = 2.0
GOAL_Y = 1.0
GOAL_YAW = 0.0

WHEEL_SEPARATION = 0.28
WHEEL_RADIUS = 0.05
ROBOT_RADIUS = 0.22
INFLATION_RADIUS = 0.30


def rectangles():
    """占用区域，轴对齐，单位米。(x0, x1, y0, y1)。"""
    half = HALF_EXTENT
    thick = WALL_THICKNESS
    regions = [
        (-half, -half + thick, -half, half),
        (half - thick, half, -half, half),
        (-half, half, -half, -half + thick),
        (-half, half, half - thick, half),
    ]
    center_x, center_y = OBSTACLE_CENTER
    size_x, size_y = OBSTACLE_SIZE
    regions.append(
        (
            center_x - size_x / 2.0,
            center_x + size_x / 2.0,
            center_y - size_y / 2.0,
            center_y + size_y / 2.0,
        )
    )
    return regions


def _overlaps(a0, a1, b0, b1):
    return a0 < b1 and a1 > b0


def cell_bounds(col, row):
    x0 = -HALF_EXTENT + col * RESOLUTION
    y0 = -HALF_EXTENT + row * RESOLUTION
    return x0, x0 + RESOLUTION, y0, y0 + RESOLUTION


def cell_occupied(col, row):
    x0, x1, y0, y1 = cell_bounds(col, row)
    for rx0, rx1, ry0, ry1 in rectangles():
        if _overlaps(x0, x1, rx0, rx1) and _overlaps(y0, y1, ry0, ry1):
            return True
    return False


def distance_to_occupied(x, y):
    """点到最近占用矩形的距离。点在矩形内时为 0。"""
    best = None
    for x0, x1, y0, y1 in rectangles():
        dx = 0.0 if x0 <= x <= x1 else min(abs(x - x0), abs(x - x1))
        dy = 0.0 if y0 <= y <= y1 else min(abs(y - y0), abs(y - y1))
        dist = (dx * dx + dy * dy) ** 0.5
        if best is None or dist < best:
            best = dist
    return 0.0 if best is None else best


def map_pixels():
    return int(round(2.0 * HALF_EXTENT / RESOLUTION))
