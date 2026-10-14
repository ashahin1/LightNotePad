import os
import math
from PIL import Image, ImageDraw, ImageFilter

def create_notepad_icon(output_ico_path, output_png_path=None):
    # Render at high resolution (1024x1024) and downsample with Lanczos for smooth antialiasing
    canvas_size = 1024
    img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Notepad boundaries
    pad_left = 180
    pad_top = 110
    pad_right = 844
    pad_bottom = 914
    radius = 64

    # 1. Multi-layered natural drop shadow
    shadow_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow_img)
    
    # Deep shadow
    s_draw.rounded_rectangle(
        [pad_left + 16, pad_top + 28, pad_right + 16, pad_bottom + 28],
        radius=radius,
        fill=(0, 0, 0, 50)
    )
    # Ambient soft shadow
    s_draw.rounded_rectangle(
        [pad_left + 8, pad_top + 16, pad_right + 8, pad_bottom + 16],
        radius=radius,
        fill=(0, 0, 0, 70)
    )
    shadow_img = shadow_img.filter(ImageFilter.GaussianBlur(radius=24))
    img.alpha_composite(shadow_img)

    # 2. Back cover (Notebook backboard peeking on the left and bottom)
    cover_offset = 12
    draw.rounded_rectangle(
        [pad_left - cover_offset, pad_top - 4, pad_right + 6, pad_bottom + cover_offset],
        radius=radius,
        fill=(14, 116, 144, 255) # Cyan-700 / Deep Teal-Blue base
    )
    # Gradient accent for back cover
    draw.rounded_rectangle(
        [pad_left - cover_offset + 2, pad_top - 2, pad_right + 4, pad_bottom + cover_offset - 2],
        radius=radius,
        fill=(3, 105, 161, 255) # Sky-700
    )

    # 3. Main Paper Page (Clean, bright white notepad surface)
    # Background paper rectangle with subtle crisp outline
    draw.rounded_rectangle(
        [pad_left, pad_top, pad_right, pad_bottom],
        radius=radius,
        fill=(255, 255, 255, 255),
        outline=(203, 213, 225, 255), # Slate-300 subtle border so it pops on white backgrounds
        width=3
    )

    # Subtle vertical paper gradient (slightly off-white at the very bottom for depth)
    paper_gradient = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    p_draw = ImageDraw.Draw(paper_gradient)
    p_draw.rounded_rectangle(
        [pad_left, pad_bottom - 120, pad_right, pad_bottom],
        radius=radius,
        fill=(241, 245, 249, 120) # Slate-100 soft gradient
    )
    # Mask to keep inside rounded rectangle
    img.alpha_composite(paper_gradient)

    # 4. Top Binding Header Strip (Classic Notepad Blue Accent)
    # Header from pad_top to pad_top + 140
    header_height = 140
    header_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    h_draw = ImageDraw.Draw(header_img)

    # Rounded top of header
    h_draw.rounded_rectangle(
        [pad_left, pad_top, pad_right, pad_top + header_height],
        radius=radius,
        fill=(0, 120, 215, 255) # Windows Classic / Fluent Azure: #0078D7
    )
    # Flatten the bottom of the header strip
    h_draw.rectangle(
        [pad_left, pad_top + radius, pad_right, pad_top + header_height],
        fill=(0, 120, 215, 255)
    )

    # Top highlight sheen (gives premium glass/gloss depth)
    h_draw.line(
        [pad_left + radius, pad_top + 4, pad_right - radius, pad_top + 4],
        fill=(255, 255, 255, 120),
        width=4
    )

    # Subtle bottom shadow under the header strip
    h_draw.line(
        [pad_left, pad_top + header_height, pad_right, pad_top + header_height],
        fill=(2, 90, 170, 255),
        width=4
    )

    img.alpha_composite(header_img)

    # 5. Spiral Binding Rings (Classic Notepad wire loops along the top)
    num_rings = 6
    ring_spacing = (pad_right - pad_left - 120) / (num_rings - 1)
    for i in range(num_rings):
        rx = int(pad_left + 60 + i * ring_spacing)
        ry_top = pad_top - 16
        ry_bottom = pad_top + 44
        # Ring shadow
        draw.rounded_rectangle(
            [rx - 10 + 3, ry_top + 3, rx + 10 + 3, ry_bottom + 3],
            radius=10,
            fill=(0, 0, 0, 60)
        )
        # Ring chrome / silver body
        draw.rounded_rectangle(
            [rx - 10, ry_top, rx + 10, ry_bottom],
            radius=10,
            fill=(226, 232, 240, 255), # Slate-200
            outline=(148, 163, 184, 255), # Slate-400
            width=2
        )
        # Inner wire highlight
        draw.line([rx - 2, ry_top + 6, rx - 2, ry_bottom - 6], fill=(255, 255, 255, 220), width=3)
        # Hole punch under the ring
        draw.ellipse([rx - 8, ry_bottom - 2, rx + 8, ry_bottom + 14], fill=(30, 41, 59, 255))

    # 6. Ruled Text Lines on the Notepad Page
    content_top = pad_top + header_height + 55
    text_left = pad_left + 70
    text_right = pad_right - 70

    # Header title line (bold, accent blue)
    draw.rounded_rectangle(
        [text_left, content_top, text_left + int((text_right - text_left) * 0.55), content_top + 20],
        radius=10,
        fill=(14, 116, 144, 255) # Cyan-700
    )

    # Paragraph lines (clean slate lines)
    line_configs = [
        (content_top + 55, 0.90, (148, 163, 184, 230), 14),
        (content_top + 100, 0.78, (148, 163, 184, 230), 14),
        (content_top + 145, 0.85, (148, 163, 184, 230), 14),
        (content_top + 190, 0.60, (148, 163, 184, 230), 14),
        (content_top + 245, 0.88, (148, 163, 184, 210), 14),
        (content_top + 290, 0.72, (148, 163, 184, 210), 14),
        (content_top + 335, 0.45, (148, 163, 184, 210), 14),
    ]

    for ly, ratio, color, thick in line_configs:
        lw = int((text_right - text_left) * ratio)
        draw.rounded_rectangle(
            [text_left, ly, text_left + lw, ly + thick],
            radius=thick // 2,
            fill=color
        )

    # 7. Modern Stylus / Pen (Inspired by original Notepad's iconic pencil/pen)
    # Drawn angled across the bottom right corner
    pen_cx, pen_cy = 710, 720
    # Render pen on separate layer with rotation for sharpness
    pen_layer = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    pen_draw = ImageDraw.Draw(pen_layer)

    # Pen points rotated at -40 degrees
    # Pen body: 420 px long, 40 px thick
    angle = -math.radians(38)
    cos_a, sin_a = math.cos(angle), math.sin(angle)

    def rot(x, y):
        # Rotate around (pen_cx, pen_cy)
        return (
            pen_cx + x * cos_a - y * sin_a,
            pen_cy + x * sin_a + y * cos_a
        )

    # Pen Shadow
    pen_shadow = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    ps_draw = ImageDraw.Draw(pen_shadow)
    
    # Shadow polygon
    s_pts = [
        rot(-170 + 16, -18 + 20),
        rot(150 + 16, -18 + 20),
        rot(200 + 16, 0 + 20),
        rot(150 + 16, 18 + 20),
        rot(-170 + 16, 18 + 20)
    ]
    ps_draw.polygon(s_pts, fill=(0, 0, 0, 70))
    pen_shadow = pen_shadow.filter(ImageFilter.GaussianBlur(radius=12))
    img.alpha_composite(pen_shadow)

    # Pen Body (Vibrant Azure & Gold / Chrome Accents)
    # Barrel: from x = -160 to x = 110
    barrel_pts = [
        rot(-160, -18),
        rot(110, -18),
        rot(110, 18),
        rot(-160, 18)
    ]
    pen_draw.polygon(barrel_pts, fill=(2, 132, 199, 255)) # Sky-600 vibrant blue

    # Barrel top highlight
    pen_draw.line([rot(-155, -9), rot(105, -9)], fill=(125, 211, 252, 220), width=6)

    # Gold / Brass band
    band_pts = [
        rot(110, -18),
        rot(128, -18),
        rot(128, 18),
        rot(110, 18)
    ]
    pen_draw.polygon(band_pts, fill=(245, 158, 11, 255)) # Amber-500 gold band

    # Cone / Grip (polished silver/slate)
    cone_pts = [
        rot(128, -17),
        rot(180, -7),
        rot(180, 7),
        rot(128, 17)
    ]
    pen_draw.polygon(cone_pts, fill=(203, 213, 225, 255)) # Slate-300 silver

    # Fine Pen Tip (dark graphite nib)
    nib_pts = [
        rot(180, -6),
        rot(206, 0),
        rot(180, 6)
    ]
    pen_draw.polygon(nib_pts, fill=(15, 23, 42, 255)) # Slate-900 nib
    # Nib gold sparkle
    pen_draw.polygon([rot(180, -3), rot(194, 0), rot(180, 3)], fill=(245, 158, 11, 255))

    # Pen Cap / End piece
    end_pts = [
        rot(-174, -15),
        rot(-160, -18),
        rot(-160, 18),
        rot(-174, 15)
    ]
    pen_draw.polygon(end_pts, fill=(14, 165, 233, 255)) # Sky-500 end cap

    img.alpha_composite(pen_layer)

    # Save high-res PNG preview if requested
    if output_png_path:
        img.save(output_png_path, "PNG")

    # Generate complete multi-size ICO from high-resolution master
    icon_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(output_ico_path, format="ICO", sizes=icon_sizes)
    print(f"Polished multi-size Notepad Icon successfully generated at: {output_ico_path}")

if __name__ == "__main__":
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    res_dir = os.path.join(tools_dir, "..", "resources")
    os.makedirs(res_dir, exist_ok=True)
    
    ico_dest = os.path.join(res_dir, "app.ico")
    png_preview = os.path.join(res_dir, "app_icon_preview.png")
    
    create_notepad_icon(ico_dest, png_preview)
