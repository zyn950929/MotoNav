import UIKit

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 把中文文字画成 4 位灰度小图片（16 级灰度，两个像素占一个字节，高 4 位是左边那个），发给圆屏显示。
/// 圆屏没有中文字库，用手机的字体画好再发过去（和安卓版格式一样）。
enum TextImage {
    struct Img { let w: Int; let h: Int; let data: Data }

    /// maxW：最大宽度（像素）；文字太长先缩小字号，还放不下就截断加省略号
    static func render(_ text: String, maxW: Int, size: CGFloat, minSize: CGFloat = 16, byteLimit: Int? = nil) -> Img? {
        var t = text.trimmingCharacters(in: .whitespaces)
        if t.isEmpty { return nil }
        var fontSize = size
        func attrs(_ s: CGFloat) -> [NSAttributedString.Key: Any] {
            [.font: UIFont.boldSystemFont(ofSize: s), .foregroundColor: UIColor.white]
        }
        func width(_ s: String, _ fs: CGFloat) -> CGFloat { (s as NSString).size(withAttributes: attrs(fs)).width }
        func height(_ fs:CGFloat)->Int {min(60,max(1,Int(ceil(UIFont.boldSystemFont(ofSize:fs).lineHeight))))}
        func limit(_ fs:CGFloat)->Int {
            byteLimit.map{TextBitmapBudget.width(maximum:maxW,height:height(fs),bytes:$0)} ?? maxW
        }
        while width(t, fontSize) > CGFloat(limit(fontSize)) && fontSize > minSize { fontSize -= 1 }
        let allowed=limit(fontSize)
        guard allowed>0 else {return nil}
        if width(t, fontSize) > CGFloat(allowed) {
            while t.count > 1 && width(t + "…", fontSize) > CGFloat(allowed) { t.removeLast() }
            t += "…"
        }
        guard width(t,fontSize)<=CGFloat(allowed) else {return nil}
        let font = UIFont.boldSystemFont(ofSize: fontSize)
        let h = min(60, max(1, Int(ceil(font.lineHeight))))
        let w = min(allowed, max(1, Int(ceil(width(t, fontSize)))))

        // 灰度画布：黑底白字，灰度值就是“浓淡”
        guard let ctx = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w,
                                  space: CGColorSpaceCreateDeviceGray(), bitmapInfo: CGImageAlphaInfo.none.rawValue)
        else { return nil }
        ctx.setFillColor(gray: 0, alpha: 1)
        ctx.fill(CGRect(x: 0, y: 0, width: w, height: h))
        ctx.translateBy(x: 0, y: CGFloat(h))            // UIKit 的坐标是从上往下
        ctx.scaleBy(x: 1, y: -1)
        UIGraphicsPushContext(ctx)
        (t as NSString).draw(at: .zero, withAttributes: attrs(fontSize))
        UIGraphicsPopContext()
        guard let buf = ctx.data?.assumingMemoryBound(to: UInt8.self) else { return nil }

        let bpr = (w + 1) / 2
        var out = [UInt8](repeating: 0, count: bpr * h)
        for y in 0..<h {
            for x in 0..<w {
                let a = Int(buf[y * w + x])
                let nib = min(15, (a + 8) / 17)
                if nib == 0 { continue }
                let i = y * bpr + (x >> 1)
                out[i] |= (x & 1 == 0) ? UInt8(nib << 4) : UInt8(nib)
            }
        }
        return Img(w: w, h: h, data: Data(out))
    }

    /// 画好直接发到圆屏某一栏；文字为空就清掉那一栏
    static func send(slot: Int, _ text: String, maxW: Int, size: CGFloat, minSize: CGFloat) {
        if let img = render(text, maxW: maxW, size: size, minSize: minSize) {
            BleLink.shared.sendImage(slot: slot, w: img.w, h: img.h, data: img.data)
        } else {
            BleLink.shared.sendImage(slot: slot, w: 0, h: 0, data: Data())
        }
    }
}
