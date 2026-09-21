"""Reuse the verified runtime signatures for the exact short-path trial assets."""
import check_v44_live_binding as check

check.EXPECTED = {
    'mesh': '/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2',
    'skeleton': '/Game/CSS/Shared/SKEL_Base.SKEL_Base',
    'physics': '/Game/CSS/SeduXtress/PA_Body.PA_Body',
}
check.ANIM_CLASS = '/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary_C'

if __name__ == '__main__':
    raise SystemExit(check.main())
