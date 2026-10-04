/* Independent test-fixture mapping of the old three-choice scenario scripts to
 * the new two-column Nova choices. Preserve travel so drags remain drags. */
#ifdef PORTABLE_NOVA_UI
static void nova_fixture_choice_coordinates(unsigned page,uint16_t *x,uint16_t *y) {
    if((page!=SV_ALERT && page!=SV_SLEEP) || *x<28 || *x>=212)return;
    if(*y>=66 && *y<96){*x=(uint16_t)(20+(*x-28)*96/184);*y-=2;}
    else if(*y>=104 && *y<134){*x=(uint16_t)(124+(*x-28)*96/184);*y-=40;}
    else if(*y>=142 && *y<172){*x=(uint16_t)(20+(*x-28)*200/184);*y-=22;}
}
#endif
