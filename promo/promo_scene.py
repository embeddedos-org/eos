"""EoS — production promo video with synced narration."""
from manim import *
import json
import os

# Load durations from generate_audio.py
dur_path = os.path.join(os.path.dirname(__file__), "durations.json")
if os.path.exists(dur_path):
    with open(dur_path) as f:
        DUR = json.load(f)
else:
    DUR = {"intro": 4, "f1": 6, "f2": 6, "f3": 6, "arch": 7, "cta": 5}

ACCENT = "#ef4444"
BG = "#0f172a"
DARK = "#1e293b"


class ProductPromo(Scene):
    def construct(self):
        self.camera.background_color = BG

        # ═══ INTRO ═══
        title = Text("EoS", font_size=96, color=WHITE, weight=BOLD)
        underline = Line(LEFT * 3, RIGHT * 3, color=ACCENT, stroke_width=4)
        underline.next_to(title, DOWN, buff=0.3)
        tagline = Text("Embedded Operating System", font_size=28, color=GRAY_B)
        tagline.next_to(underline, DOWN, buff=0.4)
        # Tech badges
        techs = "C, Assembly, ARM, RISC-V".split(", ")
        badges = VGroup()
        for t in techs:
            badge = VGroup(
                RoundedRectangle(corner_radius=0.1, width=len(t)*0.18+0.6, height=0.4,
                                 stroke_color=ACCENT, fill_color=DARK, fill_opacity=1),
                Text(t, font_size=14, color=WHITE),
            )
            badge[1].move_to(badge[0])
            badges.add(badge)
        badges.arrange(RIGHT, buff=0.3).next_to(tagline, DOWN, buff=0.5)

        self.play(Write(title), run_time=0.8)
        self.play(Create(underline), FadeIn(tagline, shift=UP*0.2), run_time=0.6)
        self.play(LaggedStart(*[FadeIn(b, scale=0.8) for b in badges], lag_ratio=0.1), run_time=0.6)
        self.wait(DUR["intro"] - 2.0)
        self.play(FadeOut(VGroup(title, underline, tagline, badges)), run_time=0.4)

        # ═══ FEATURES ═══
        features = [
            ("01", "Micro-Kernel Architecture", "Minimal trusted core with drivers and services running in user space", DUR["f1"]),
            ("02", "Real-Time Scheduling", "Priority-based preemptive scheduler guarantees deterministic task deadlines", DUR["f2"]),
            ("03", "Hardware Abstraction Layer", "Portable HAL supports ARM Cortex-M, RISC-V, and x86 architectures", DUR["f3"]),
        ]
        for num, feat_name, feat_desc, dur in features:
            # Large number watermark
            num_text = Text(num, font_size=200, color=ACCENT, weight=BOLD,
                           font="Monospace").set_opacity(0.08)
            num_text.to_edge(LEFT, buff=0.5)

            # Feature title
            feat_title = Text(feat_name, font_size=48, color=WHITE, weight=BOLD)
            feat_title.to_edge(UP, buff=1.5).shift(RIGHT * 0.5)

            # Accent bar
            bar = Rectangle(width=6, height=0.05, color=ACCENT, fill_opacity=1)
            bar.next_to(feat_title, DOWN, buff=0.2, aligned_edge=LEFT)

            # Description text (wrapped)
            desc_text = Paragraph(
                feat_desc, font_size=22, color=GRAY_B,
                line_spacing=1.2, alignment="left",
            ).scale(0.9)
            desc_text.next_to(bar, DOWN, buff=0.4, aligned_edge=LEFT)
            if desc_text.width > 10:
                desc_text.scale(10 / desc_text.width)

            # Visual element: tech diagram box
            diagram = VGroup(
                RoundedRectangle(corner_radius=0.15, width=4, height=2.5,
                                 stroke_color=ACCENT, stroke_width=1,
                                 fill_color=DARK, fill_opacity=0.5),
            )
            # Add icon-like dots inside
            for row in range(3):
                for col in range(4):
                    dot = Dot(radius=0.04, color=ACCENT).set_opacity(0.3 + row*0.2)
                    dot.move_to(diagram[0].get_center() + RIGHT*(col-1.5)*0.6 + DOWN*(row-1)*0.5)
                    diagram.add(dot)
            diagram.to_edge(RIGHT, buff=1).shift(DOWN * 0.3)

            grp = VGroup(num_text, feat_title, bar, desc_text, diagram)
            self.play(
                FadeIn(num_text),
                Write(feat_title), GrowFromEdge(bar, LEFT),
                run_time=0.7,
            )
            self.play(FadeIn(desc_text, shift=UP*0.2), FadeIn(diagram, scale=0.9), run_time=0.6)
            self.wait(dur - 1.7)
            self.play(FadeOut(grp), run_time=0.4)

        # ═══ ARCHITECTURE ═══
        arch_label = Text("Architecture", font_size=20, color=GRAY_B)
        arch_label.to_edge(UP, buff=0.6)

        components = ["Kernel", "Scheduler", "Memory Mgr", "Driver HAL", "Syscall"]
        boxes = VGroup()
        for i, comp in enumerate(components):
            box = VGroup(
                RoundedRectangle(
                    corner_radius=0.12, width=2.2, height=1.0,
                    stroke_color=ACCENT, fill_color=DARK, fill_opacity=1, stroke_width=2,
                ),
                Text(comp, font_size=16, color=WHITE),
            )
            box[1].move_to(box[0])
            boxes.add(box)
        boxes.arrange(RIGHT, buff=0.4)

        arrows = VGroup()
        for i in range(len(boxes) - 1):
            arr = Arrow(
                boxes[i].get_right(), boxes[i+1].get_left(),
                color=ACCENT, buff=0.08, stroke_width=2,
                max_tip_length_to_length_ratio=0.15,
            )
            arrows.add(arr)

        # Data flow dots
        flow_dots = VGroup()
        for arr in arrows:
            for t in [0.3, 0.5, 0.7]:
                dot = Dot(radius=0.03, color=ACCENT).set_opacity(0.6)
                dot.move_to(arr.point_from_proportion(t))
                flow_dots.add(dot)

        self.play(FadeIn(arch_label), run_time=0.3)
        self.play(
            LaggedStart(*[FadeIn(b, shift=UP*0.3) for b in boxes], lag_ratio=0.12),
            run_time=0.8,
        )
        self.play(
            LaggedStart(*[GrowArrow(a) for a in arrows], lag_ratio=0.1),
            run_time=0.5,
        )
        self.play(LaggedStart(*[FadeIn(d, scale=0) for d in flow_dots], lag_ratio=0.05), run_time=0.4)
        self.wait(DUR["arch"] - 2.4)
        self.play(FadeOut(VGroup(arch_label, boxes, arrows, flow_dots)), run_time=0.4)

        # ═══ CTA ═══
        cta_name = Text("EoS", font_size=72, color=WHITE, weight=BOLD)
        cta_line = Line(LEFT*2, RIGHT*2, color=ACCENT, stroke_width=3)
        cta_line.next_to(cta_name, DOWN, buff=0.3)
        cta_url = Text(
            "github.com/embeddedos-org/eos",
            font_size=22, color=ManimColor(ACCENT),
        )
        cta_url.next_to(cta_line, DOWN, buff=0.3)
        cta_badge = Text("Open Source  ·  MIT License  ·  Production Ready",
                         font_size=16, color=GRAY_B)
        cta_badge.next_to(cta_url, DOWN, buff=0.3)
        star = Text("★  Star us on GitHub", font_size=18, color=YELLOW).set_opacity(0.8)
        star.next_to(cta_badge, DOWN, buff=0.4)

        self.play(Write(cta_name), Create(cta_line), run_time=0.7)
        self.play(FadeIn(cta_url, shift=UP*0.2), run_time=0.4)
        self.play(FadeIn(cta_badge), FadeIn(star), run_time=0.4)
        self.wait(DUR["cta"] - 1.9)
