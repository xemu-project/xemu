include!("../../html2win32_name.rs");
include!("../../html2win32.rs");
include!("../../osx2win32_name.rs");
include!("../../osx2win32.rs");
include!("../../osx2xkb_name.rs");
include!("../../osx2xkb.rs");
include!("../../osx_name.rs");
include!("../../osx.rs");

fn main() {
    assert_eq!(CODE_MAP_OSX_TO_WIN32[0x1d], 0x30);
    assert_eq!(NAME_MAP_OSX_TO_WIN32[0x1d], "VK_0");

    assert_eq!(CODE_MAP_OSX_TO_XKB[0x1d], "AE10");
    assert_eq!(NAME_MAP_OSX_TO_XKB[0x1d], "AE10");

    assert_eq!(CODE_MAP_HTML_TO_WIN32["ControlLeft"], 0x11);
    assert_eq!(NAME_MAP_HTML_TO_WIN32["ControlLeft"], "VK_CONTROL");

    assert_eq!(CODE_TABLE_OSX[0x1d], 0x3b);
    assert_eq!(NAME_TABLE_OSX[0x1d], "Control");
}


#[test]
fn test() {
    main()
}
