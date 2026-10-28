from pathlib import Path
import zipfile
root = Path(__file__).resolve().parents[1]
output = root.parent / 'dist-activation'
output.mkdir(exist_ok=True)
with zipfile.ZipFile(output / 'NOORI-Activation-Hostinger.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for folder, target in [('php/public', 'noori-activation'), ('php/private', 'noori-private')]:
        for source in (root / folder).iterdir():
            if not source.is_file():
                continue
            if source.name not in {'bootstrap.php','index.php','admin.php','setup.php','.htaccess',
                                   'service.php','schema.mysql.sql','config.example.php'}:
                continue
            archive.write(source, target + '/' + source.name)
    archive.write(root / 'HOSTINGER-SETUP.md', 'HOSTINGER-SETUP.md')
    archive.write(root / 'README.md', 'README.md')
print('Created hosting source bundle; no production credentials or private signing key included.')
