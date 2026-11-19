// glyphdump <ttf> <보조 ttf> <out>: WIE의 DrawString과 같은 방식(ab_glyph, 10pt)으로 CP949 글자를 래스터화해 표로 남긴다.
// 첫 글꼴(neodgm)에 없는 글자는 보조 글꼴(Galmuri, KS X 1001 전체)에서 같은 크기로 가져온다. 원래 폰 글꼴은 KS X 1001을 다 그렸다.
// 제어 문자도 넣는다. 그리지는 않지만 GetStringWidth는 그 전진 폭까지 더한다.
// 형식(리틀 엔디언): "GLY1", 개수 u32, 그 뒤 항목마다 코드 u16(1바이트 글자는 0x00XX), 유니코드 u32, 전진 f32,
// 경계 min.x f32, min.y f32, 폭 u16, 높이 u16, 커버리지 폭*높이 바이트(c*255를 u8로 자른 값).
use ab_glyph::{Font, FontRef, ScaleFont};
use std::io::Write;

const FONT_SIZE: f32 = 10.0;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let data = std::fs::read(&args[1]).expect("ttf");
    let font = FontRef::try_from_slice(&data).expect("font");
    let scaled = font.as_scaled(font.pt_to_px_scale(FONT_SIZE).unwrap());
    let fdata = std::fs::read(&args[2]).expect("fallback ttf");
    let fallback = FontRef::try_from_slice(&fdata).expect("fallback font");
    let fscaled = fallback.as_scaled(fallback.pt_to_px_scale(FONT_SIZE).unwrap());
    let mut borrowed = 0;
    let mut codes: Vec<(u16, char)> = Vec::new();
    for b in 0x00u16..0x80 {
        codes.push((b, b as u8 as char));
    }
    for lead in 0x81u16..=0xfe {
        for trail in 0x41u16..=0xfe {
            let bytes = [lead as u8, trail as u8];
            let (s, had_errors) = encoding_rs::EUC_KR.decode_without_bom_handling(&bytes);
            let mut it = s.chars();
            if let (Some(c), None, false) = (it.next(), it.next(), had_errors) {
                codes.push((lead << 8 | trail, c));
            }
        }
    }
    codes.push((0xfffd, '\u{fffd}'));
    let mut out = Vec::new();
    out.extend_from_slice(b"GLY1");
    out.extend_from_slice(&(codes.len() as u32).to_le_bytes());
    for (code, c) in codes {
        let own = font.glyph_id(c).0 != 0 || fallback.glyph_id(c).0 == 0;
        if !own {
            borrowed += 1;
        }
        let sf: &dyn ScaleFontDyn = if own { &scaled } else { &fscaled };
        let glyph = sf.glyph(c);
        let adv = sf.advance(glyph.id);
        out.extend_from_slice(&code.to_le_bytes());
        out.extend_from_slice(&(c as u32).to_le_bytes());
        out.extend_from_slice(&adv.to_le_bytes());
        match sf.outline(glyph) {
            Some(g) => {
                let b = g.px_bounds();
                let (w, h) = (b.width() as u32, b.height() as u32);
                let mut cov = vec![0u8; (w * h) as usize];
                g.draw(|x, y, v| {
                    if x < w && y < h {
                        cov[(y * w + x) as usize] = (v * 255.0) as u8;
                    }
                });
                out.extend_from_slice(&b.min.x.to_le_bytes());
                out.extend_from_slice(&b.min.y.to_le_bytes());
                out.extend_from_slice(&(w as u16).to_le_bytes());
                out.extend_from_slice(&(h as u16).to_le_bytes());
                out.extend_from_slice(&cov);
            }
            None => {
                out.extend_from_slice(&0f32.to_le_bytes());
                out.extend_from_slice(&0f32.to_le_bytes());
                out.extend_from_slice(&0u16.to_le_bytes());
                out.extend_from_slice(&0u16.to_le_bytes());
            }
        }
    }
    std::fs::File::create(&args[3]).unwrap().write_all(&out).unwrap();
    eprintln!("{} bytes, 보조 글꼴에서 {}자", out.len(), borrowed);
}

// 두 글꼴의 ScaleFont를 같은 자리에서 쓰기 위한 얇은 감싸기.
trait ScaleFontDyn {
    fn glyph(&self, c: char) -> ab_glyph::Glyph;
    fn advance(&self, id: ab_glyph::GlyphId) -> f32;
    fn outline(&self, g: ab_glyph::Glyph) -> Option<ab_glyph::OutlinedGlyph>;
}
impl<'a> ScaleFontDyn for ab_glyph::PxScaleFont<&'a FontRef<'a>> {
    fn glyph(&self, c: char) -> ab_glyph::Glyph { self.scaled_glyph(c) }
    fn advance(&self, id: ab_glyph::GlyphId) -> f32 { self.h_advance(id) }
    fn outline(&self, g: ab_glyph::Glyph) -> Option<ab_glyph::OutlinedGlyph> { self.outline_glyph(g) }
}
