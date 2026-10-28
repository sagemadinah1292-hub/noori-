"""Source-only packages for initial Hostinger setup; no live configuration or keys."""
from pathlib import Path
import zipfile
root=Path(__file__).resolve().parents[1]
output=root.parent/'dist-activation';output.mkdir(exist_ok=True)
public_names=['bootstrap.php','index.php','index.html','admin.php','setup.php','.htaccess','portal-bootstrap.php','home.php','login.php','account.php','register.php','create-password.php','dashboard.php','download.php','logout.php','change-password.php','reset-password.php','secure-downloads.php']
private_names=['service.php','portal.php','portal-deploy.php','schema.mysql.sql','config.example.php','.htaccess']
assets=['site.css','noori-logo.svg']
with zipfile.ZipFile(output/'NOORI-Activation-Hostinger.zip','w',zipfile.ZIP_DEFLATED) as z:
    for folder,target,names in [('php/public','noori-activation',public_names),('php/private','noori-private',private_names)]:
        for name in names:z.write(root/folder/name,target+'/'+name)
    for name in assets:z.write(root/'php/public/assets'/name,'noori-activation/assets/'+name)
    z.write(root/'HOSTINGER-SETUP.md','HOSTINGER-SETUP.md');z.write(root/'README.md','README.md')
for archive,folder,names in [('NOORI-Activation-Public.zip','php/public',public_names),('NOORI-Activation-Private.zip','php/private',private_names)]:
    with zipfile.ZipFile(output/archive,'w',zipfile.ZIP_DEFLATED) as z:
        for name in names:z.write(root/folder/name,name)
        if folder=='php/public':
            for name in assets:z.write(root/'php/public/assets'/name,'assets/'+name)
print('Created initial-setup source archives. Existing live sites should use the Final Public/Private Upgrade ZIPs instead.')
