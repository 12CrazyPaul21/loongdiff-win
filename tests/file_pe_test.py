import ldiff_test_common as tcommon

tcommon.init()

work_folder = tcommon.create_random_folder()
source = tcommon.generate_random_file(parent=work_folder)
target = tcommon.generate_random_file(source['filename'], parent=work_folder)

print('source info : ', source)
print('target info :', target)

patch_info = tcommon.generate_pe_patch(source, target, parent_folder=work_folder)
print('diff result : ', patch_info)
if patch_info['code'] != 0:
    tcommon.failed(patch_info['code'])

# multi times will do nothing
for i in range(2):
    apply_info = tcommon.apply_pe_patch(source, patch_info)
    print('apply result : ', apply_info)
    if apply_info['code'] != 0:
        tcommon.failed(apply_info['code'])

    if apply_info['filehash'] != target['filehash']:
        tcommon.failed(tcommon.ExitCode.ApplyFailed)

    if apply_info['backupsize'] == 0:
        tcommon.failed(tcommon.ExitCode.BackupFailed)

# diff again
patch_info = tcommon.generate_pe_patch(source, target, parent_folder=work_folder)
if patch_info['code'] != 2: # 2 -> needless
    tcommon.failed(patch_info['code'])

tcommon.remove_dirs([work_folder])

tcommon.success()