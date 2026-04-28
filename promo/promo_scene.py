from manim import *


class ProductPromo(Scene):
    """EoS — promotional video."""

    def construct(self):
        self.camera.background_color = "#0f172a"
        accent = "#ef4444"

        # ── Scene 1: Logo Reveal ──
        title = Text("EoS", font_size=96, color=WHITE, weight=BOLD)
        tagline = Text(
            "Embedded Operating System",
            font_size=32, color=GRAY_B,
        )
        tagline.next_to(title, DOWN, buff=0.6)

        self.play(Write(title), run_time=1.5)
        self.play(FadeIn(tagline, shift=UP * 0.3), run_time=0.8)
        self.wait(2)
        self.play(FadeOut(title), FadeOut(tagline))

        # ── Scene 2: Feature Showcase ──
        features = [
            "Micro-Kernel Architecture",
            "Real-Time Scheduling",
            "Hardware Abstraction Layer",
        ]
        for i, feat in enumerate(features):
            num = Text(
                f"0{i+1}", font_size=120, color=accent, weight=BOLD, font="Monospace",
            ).set_opacity(0.15)
            num.to_edge(LEFT, buff=1.5).shift(UP * 0.5)

            feat_text = Text(feat, font_size=52, color=WHITE, weight=BOLD)
            feat_text.next_to(num, RIGHT, buff=0.8).align_to(num, UP)

            bar = Rectangle(
                width=8, height=0.06, color=accent, fill_opacity=1,
            )
            bar.next_to(feat_text, DOWN, buff=0.3, aligned_edge=LEFT)

            self.play(
                FadeIn(num, shift=RIGHT * 0.5),
                Write(feat_text),
                GrowFromEdge(bar, LEFT),
                run_time=1.0,
            )
            self.wait(1.5)
            self.play(FadeOut(num), FadeOut(feat_text), FadeOut(bar), run_time=0.5)

        # ── Scene 3: Architecture Flash ──
        arch_title = Text("Architecture", font_size=28, color=GRAY_B)
        arch_title.to_edge(UP, buff=0.8)

        boxes = VGroup()
        labels = ["Core", "API", "Runtime"]
        for j, lbl in enumerate(labels):
            box = RoundedRectangle(
                corner_radius=0.15, width=2.5, height=1.2,
                stroke_color=accent, fill_color="#1e293b", fill_opacity=1,
            )
            box_label = Text(lbl, font_size=22, color=WHITE)
            box_label.move_to(box)
            grp = VGroup(box, box_label)
            boxes.add(grp)
        boxes.arrange(RIGHT, buff=0.6)

        arrows = VGroup()
        for j in range(len(boxes) - 1):
            arr = Arrow(
                boxes[j].get_right(), boxes[j + 1].get_left(),
                color=accent, buff=0.1, stroke_width=2,
            )
            arrows.add(arr)

        self.play(FadeIn(arch_title))
        self.play(LaggedStart(*[FadeIn(b, shift=UP * 0.3) for b in boxes], lag_ratio=0.2))
        self.play(LaggedStart(*[GrowArrow(a) for a in arrows], lag_ratio=0.15))
        self.wait(2)
        self.play(FadeOut(arch_title), FadeOut(boxes), FadeOut(arrows))

        # ── Scene 4: CTA ──
        cta = Text("EoS", font_size=72, color=WHITE, weight=BOLD)
        url = Text(
            "github.com/embeddedos-org/eos",
            font_size=24, color=accent,
        )
        url.next_to(cta, DOWN, buff=0.5)
        badge = Text("Open Source", font_size=18, color=GRAY_B)
        badge.next_to(url, DOWN, buff=0.4)

        self.play(Write(cta), run_time=1.0)
        self.play(FadeIn(url, shift=UP * 0.2))
        self.play(FadeIn(badge, shift=UP * 0.2))
        self.wait(3)
