from pathlib import Path
import json, os, pwd, shutil, time
user = pwd.getpwnam('n1')
config = Path('/home/n1/.config/wiliwili')
config.mkdir(parents=True, exist_ok=True)
settings = config/'wiliwili_config.json'
if settings.exists():
    shutil.copy2(settings, settings.with_suffix('.json.before-install-'+time.strftime('%Y%m%d-%H%M%S')))
    data=json.loads(settings.read_text())
else:
    data={}
data.setdefault('setting',{}).update({'player_hwdec':True,'player_low_quality':False,'player_hwdec_custom':'v4l2m2m-copy','video_codec':7,'video_quality':64,'file_format':4048,'fullscreen':False})
settings.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
mpv=config/'mpv.conf'
if mpv.exists():shutil.copy2(mpv,mpv.with_suffix('.conf.before-install-'+time.strftime('%Y%m%d-%H%M%S')))
mpv.write_text('''# N1 GLES2 playback defaults
hwdec-codecs=h264
vd-lavc-o=num_capture_buffers=24,n1_copy_frames=0
gpu-dumb-mode=yes
scale=bilinear
cscale=bilinear
dscale=bilinear
dither-depth=no
ao=pulse
audio-channels=stereo
input-ipc-server=/run/user/1000/wiliwili-mpv.sock
''')
for p in [config,settings,mpv]:
    os.chown(p,user.pw_uid,user.pw_gid)
settings.chmod(0o600)
Path('/usr/local/bin/n1-wiliwili').write_bytes(Path('/opt/wiliwili-build/n1-wiliwili').read_bytes())
Path('/usr/local/bin/n1-wiliwili').chmod(0o755)
entry='''[Desktop Entry]
Type=Application
Name=wiliwili
Comment=哔哩哔哩客户端
Exec=/usr/local/bin/n1-wiliwili
Icon=/opt/wiliwili/share/icons/hicolor/scalable/apps/cn.xfangfang.wiliwili.svg
Terminal=false
Categories=AudioVideo;Video;
StartupWMClass=wiliwili
'''
Path('/usr/local/share/applications').mkdir(parents=True,exist_ok=True)
Path('/usr/local/share/applications/wiliwili.desktop').write_text(entry)
# Avoid showing both the upstream entry and the N1 launcher in the app menu.
upstream=Path('/opt/wiliwili/share/applications/cn.xfangfang.wiliwili.desktop')
if upstream.exists():upstream.write_text(upstream.read_text()+'\nNoDisplay=true\n')
for desktop in [Path('/home/n1/Desktop'),Path('/home/n1/桌面')]:
    if desktop.is_dir():
        p=desktop/'wiliwili.desktop';p.write_text(entry);p.chmod(0o755);os.chown(p,user.pw_uid,user.pw_gid)
print('Configured wiliwili launcher, desktop entry and H.264 hardware decoding defaults.')

# Preserve any calibrated mapping already installed by the user.
mapping_source=Path("/opt/wiliwili-build/gamecontrollerdb.txt")
mapping_target=config/"gamecontrollerdb.txt"
if mapping_source.exists() and not mapping_target.exists():
    shutil.copyfile(mapping_source,mapping_target)
    os.chown(mapping_target,user.pw_uid,user.pw_gid)
