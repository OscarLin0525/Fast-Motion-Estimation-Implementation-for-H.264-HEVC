# y4m_to_yuv.py
import sys

def main():
    if len(sys.argv) < 3:
        print("Usage: python y4m_to_yuv.py input.y4m output.yuv")
        return
    inp, outp = sys.argv[1], sys.argv[2]
    with open(inp, "rb") as f, open(outp, "wb") as o:
        header = f.readline()
        if not header.startswith(b"YUV4MPEG2"):
            raise SystemExit("Not a Y4M file")
        parts = header.split()
        w = h = None
        for p in parts:
            if p.startswith(b"W"): w = int(p[1:])
            if p.startswith(b"H"): h = int(p[1:])
        if not w or not h:
            raise SystemExit("Width/Height not found")
        frame_size = w * h + 2 * (w * h // 4)
        while True:
            tag = f.readline()
            if not tag:
                break
            if not tag.startswith(b"FRAME"):
                raise SystemExit("Bad frame marker")
            frame = f.read(frame_size)
            if len(frame) != frame_size:
                break
            o.write(frame)

if __name__ == "__main__":
    main()
