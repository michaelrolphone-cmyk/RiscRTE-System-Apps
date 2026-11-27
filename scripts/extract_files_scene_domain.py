"""Extract original Files domain, removing only legacy presentation/controller.
Run from repository root; source transformations are recorded and reproducible.
"""
from pathlib import Path
import re,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
def remove_function(s,name):
    p=re.compile(r'^static[^\n;{}]*\b'+re.escape(name)+r'\([^;{}]*\)\s*\{',re.M)
    m=p.search(s)
    if not m:return s
    i=m.end();depth=1;quote=None;comment=None
    while depth:
        c=s[i];n=s[i:i+2]
        if comment=='line':
            if c=='\n':comment=None
        elif comment=='block':
            if n=='*/':comment=None;i+=1
        elif quote:
            if c=='\\':i+=1
            elif c==quote:quote=None
        elif n=='//':comment='line';i+=1
        elif n=='/*':comment='block';i+=1
        elif c in "\"'":quote=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
        i+=1
    return s[:m.start()]+s[i:]
removed={'portable':['fb_draw','fb_touch','fb_open','fb_close_for_return','file_browser_controller_main'], 'sharing':['fsh_draw','fsh_touch'],'peer':['fpeer_draw','fpeer_touch','fpeer_label'],'setup':['fsh_setup_draw','fsh_setup_touch'],'shared_input':['fb_text_draw']}
manifest=[]
for part in ['portable','operations','sharing','peer','setup','mount','source','shared_input']:
    src=ROOT/'Apps'/('file_browser_'+part+'.inc');s=src.read_text()
    if part=='portable':
        # Exclude final legacy entry points; semantic root supplies app_main.
        s=s[:s.index('#ifdef PORTABLE_NATIVE_CUSTODY_FENCE\nstatic void file_browser_controller_main')]
    for name in removed.get(part,[]):s=remove_function(s,name)
    s=s.replace('fb_app->millis()','files_scene_millis()')
    for p in ['operations','sharing','peer','setup','mount','source','shared_input']:
        s=s.replace('"file_browser_'+p+'.inc"','"files_scene_'+p+'.inc"')
    if part=='portable':
        for h in ['T5AppApi.h','PortableNovaUi.h','PaperPresentation.h']:
            s=s.replace('#include "'+h+'"','')
        s=s.replace('static const t5_app_api_v1 *fb_app;','')
        s=s.replace('#include "file_browser_paper.inc"','').replace('#include "file_browser_rgb_operations.inc"','')
        s=re.sub(r'#ifdef PORTABLE_FILE_BROWSER_RGB_ONLY\nstatic const paper_presentation \*const fb_paper=NULL;\n#else\nstatic const paper_presentation \*fb_paper;\n#endif','',s)
        s=s.replace('static void fb_draw(void);','').replace('static void fb_text_step(void),fb_text_draw(void);','static void fb_text_step(void);')
        s=s.replace('static bool fsh_close(void),fsh_back(void),fsh_touch(int,int);','static bool fsh_close(void),fsh_back(void);').replace('static void fsh_tick(void),fsh_open(void),fsh_draw(void),fsh_action(void);','static void fsh_tick(void),fsh_open(void),fsh_action(void);')
        s=s.replace('if(!fb_runtime->request_launch(FILE_BROWSER_RETURN_APP)){fb_message("Return failed. Retry Back.");return false;}','files_scene_exit_requested=true;')
    if part=='sharing':
        s=s.replace('static bool fpeer_close(void),fpeer_touch(int,int);','static bool fpeer_close(void);').replace('static void fpeer_tick(uint64_t),fpeer_draw(void),fsh_start(void);','static void fpeer_tick(uint64_t),fsh_start(void);')
    if part=='shared_input':
        s=s.replace('=fbp_contact_down','').replace('fbp_clean=true;','')
    out=ROOT/'Apps'/('files_scene_domain.h' if part=='portable' else 'files_scene_'+part+'.inc')
    out.write_text('/* Extracted domain from '+src.name+'; semantic UI/lifecycle supplied by files_scene.c. */\n'+s)
    manifest.append({'source':str(src.relative_to(ROOT)),'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'output':str(out.relative_to(ROOT)),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'removed_functions':removed.get(part,[])})
(ROOT/'docs'/'files-scene-extraction.json').write_text(json.dumps(manifest,indent=2)+'\n')
