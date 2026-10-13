#!/usr/bin/env python3
"""Generate a 3-page test PDF (valid xref) for verifying PDFReader."""
import sys

PAGE_W, PAGE_H = 595, 842

def make_pdf(path):
    pages = []
    pages.append(b"BT /F1 40 Tf 72 700 Td (PDFReader Test) Tj ET\n"
                 b"BT /F1 20 Tf 72 660 Td (Page 1 of 3) Tj ET")
    lines = ["Line A: native MuPDF engine", "Line B: window + F11 presentation", "Line C: cross-compiled"]
    s = b""
    y = 720
    for ln in lines:
        s += b"BT /F1 24 Tf 72 %d Td (%s) Tj ET\n" % (y, ln.encode())
        y -= 60
    pages.append(s)
    pages.append(b"BT /F1 60 Tf 150 420 Td (THE END) Tj ET")

    objs = [b"<< /Type /Catalog /Pages 2 0 R >>",
            b"<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>"]
    for i, c in enumerate(pages):
        objs.append(b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
                    b"/Resources << /Font << /F1 5 0 R >> >> /Contents %d 0 R >>" % (4 + 2*i))
        objs.append(b"<< /Length %d >>\nstream\n" % len(c) + c + b"\nendstream")
    objs.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")

    out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for i, o in enumerate(objs, 1):
        offsets.append(len(out))
        out += b"%d 0 obj\n" % i + o + b"\nendobj\n"
    xref_pos = len(out)
    out += b"xref\n0 %d\n" % (len(objs) + 1)
    out += b"0000000000 65535 f \n"
    for off in offsets[1:]:
        out += b"%010d 00000 n \n" % off
    out += (b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n"
            % (len(objs) + 1, xref_pos))
    open(path, "wb").write(out)
    print(f"wrote {path} ({len(out)} bytes, 3 pages)")

if __name__ == "__main__":
    make_pdf(sys.argv[1] if len(sys.argv) > 1 else "test.pdf")
