"""Generate narration audio using Google Text-to-Speech."""
from gtts import gTTS

NARRATION = (
    "Introducing EoS. The embedded operating system built from the ground up. Feature one: Micro-kernel architecture keeps the core minimal and secure. Feature two: Real-time scheduling guarantees deterministic task execution. Feature three: Hardware abstraction layer supports ARM, RISC-V, and x86. EoS. Open source and production ready. Visit github dot com slash embeddedos-org slash eos."
)

tts = gTTS(text=NARRATION, lang="en", slow=False)
tts.save("narration.mp3")
print(f"Generated narration.mp3 ({len(NARRATION)} chars)")
