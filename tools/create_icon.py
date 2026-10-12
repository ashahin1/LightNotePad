import os
from PIL import Image, ImageDraw

def generate_lightnotepad_icon(output_path):
    size = 512
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Notepad base coordinates
    left, top, right, bottom = 90, 50, 422, 462
    corner_radius = 42

    # Drop shadow
    shadow_offset = 12
    draw.rounded_rectangle(
        [left + shadow_offset, top + shadow_offset, right + shadow_offset, bottom + shadow_offset],
        radius=corner_radius,
        fill=(0, 0, 0, 95)
    )

    # Main Notepad Body: Sleek dark slate / deep navy background
    # Primary slate: #1E293B, gradient-ish look with subtle bottom
    body_color = (30, 41, 59, 255) # Tailwind Slate-800
    border_color = (71, 85, 105, 255) # Slate-600

    draw.rounded_rectangle(
        [left, top, right, bottom],
        radius=corner_radius,
        fill=body_color,
        outline=border_color,
        width=4
    )

    # Top header bar (like a spiral binding / title strip)
    header_height = 80
    draw.rounded_rectangle(
        [left, top, right, top + header_height],
        radius=corner_radius,
        fill=(15, 23, 42, 255) # Slate-900
    )
    # Fix the bottom curve of the header strip
    draw.rectangle(
        [left, top + corner_radius, right, top + header_height],
        fill=(15, 23, 42, 255)
    )

    # Subtle neon/cyan lightning or edit accent bar at the header edge
    accent_color = (6, 182, 212, 255) # Cyan-500
    draw.line([left + 20, top + header_height, right - 20, top + header_height], fill=accent_color, width=4)

    # Three binding dots or tabs in the header
    dot_y = top + 40
    for dot_x in [left + 50, left + 90, left + 130]:
        draw.ellipse([dot_x - 6, dot_y - 6, dot_x + 6, dot_y + 6], fill=(148, 163, 184, 255))

    # Clean text lines representation on the notepad page
    line_left = left + 50
    line_right = right - 50
    line_color = (148, 163, 184, 200) # Slate-400
    bright_line = (226, 232, 240, 240) # Slate-200

    lines = [
        (top + 130, 0.75, bright_line, 8),
        (top + 175, 0.90, line_color, 6),
        (top + 215, 0.60, line_color, 6),
        (top + 255, 0.85, line_color, 6),
        (top + 295, 0.45, line_color, 6),
    ]

    for y, width_ratio, col, thickness in lines:
        w = int((line_right - line_left) * width_ratio)
        draw.rounded_rectangle([line_left, y, line_left + w, y + thickness], radius=thickness // 2, fill=col)

    # Lightning Bolt Symbol in bottom right corner (Fast as lightning!)
    # Lightning coordinates centered around (340, 360)
    lightning_points = [
        (335, 300),
        (295, 365),
        (330, 365),
        (315, 430),
        (375, 345),
        (340, 345),
    ]
    # Glow / shadow for lightning
    draw.polygon([(x + 2, y + 2) for x, y in lightning_points], fill=(0, 0, 0, 120))
    # Lightning vibrant yellow / electric amber: #FACC15
    draw.polygon(lightning_points, fill=(250, 204, 21, 255))

    # Generate multi-size ICO
    icon_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(output_path, format="ICO", sizes=icon_sizes)
    print(f"Icon successfully generated at: {output_path}")

if __name__ == "__main__":
    out_dir = os.path.join(os.path.dirname(__file__), "..", "resources")
    os.makedirs(out_dir, exist_ok=True)
    generate_lightnotepad_icon(os.path.join(out_dir, "app.ico"))
