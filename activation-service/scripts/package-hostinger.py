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
            if source.name not in {'bootstrap.php','index.php','index.html','admin.php','setup.php','.htaccess',
                                   'service.php','schema.mysql.sql','config.example.php'}:
                continue
            archive.write(source, target + '/' + source.name)
    for source in (root / 'php/public/assets').iterdir():
        if source.is_file() and source.name in {'site.css', 'noori-logo.svg'}:
            archive.write(source, 'noori-activation/assets/' + source.name)
    archive.write(root / 'HOSTINGER-SETUP.md', 'HOSTINGER-SETUP.md')
    archive.write(root / 'README.md', 'README.md')
print('Created hosting source bundle; no production credentials or private signing key included.')

with zipfile.ZipFile(output / 'NOORI-Page-Only.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    public = root / 'php/public'
    for name in ['index.html', '.htaccess', 'assets/site.css', 'assets/noori-logo.svg']:
        archive.write(public / name, name)
print('Created page-only ZIP for an existing noori-activation folder; API/config files are not included.')

# Separate uploads let owners add activation without replacing the live download page.
for name, folder, allowlist in [
    ('NOORI-Activation-Public.zip', 'php/public', ['bootstrap.php', 'index.php', 'admin.php', 'setup.php', '.htaccess']),
    ('NOORI-Activation-Private.zip', 'php/private', ['config.example.php', 'service.php', 'schema.mysql.sql', '.htaccess']),
]:
    with zipfile.ZipFile(output / name, 'w', zipfile.ZIP_DEFLATED) as archive:
        for filename in allowlist:
            archive.write(root / folder / filename, filename)
print('Created separate public/private activation uploads; no homepage, downloads, production config or private signing key included.')
