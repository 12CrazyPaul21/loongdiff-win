import ldiff_test_common as tcommon

tcommon.init()

number = 1

def usecase(source_size, target_size):
    global number

    print(f'small file usecase {number}')
    print('----------------------------------------------------')
    print(f'source size : {source_size}, target size : {target_size}')

    work_folder = tcommon.create_random_folder()
    source = tcommon.generate_random_file(filesize=source_size, parent=work_folder)
    target = tcommon.generate_random_file(source['filename'], filesize=target_size, parent=work_folder)

    print('source info : ', source)
    print('target info :', target)

    patch_info = tcommon.generate_patch(source, target, parent_folder=work_folder)
    print('diff result : ', patch_info)
    if patch_info['code'] != 0:
        tcommon.failed(patch_info['code'])

    apply_info = tcommon.apply_patch(source, patch_info)
    print('apply result : ', apply_info)
    if apply_info['code'] != 0:
        tcommon.failed(apply_info['code'])

    if apply_info['filehash'] != target['filehash']:
        tcommon.failed(tcommon.ExitCode.ApplyFailed)

    if apply_info['backupsize'] == 0:
        tcommon.failed(tcommon.ExitCode.BackupFailed)

    print('----------------------------------------------------')

    number += 1
    tcommon.remove_dirs([work_folder])

for i in range(8):
    usecase(i, 0)
    usecase(0, i)
    usecase(i, i)

tcommon.success()