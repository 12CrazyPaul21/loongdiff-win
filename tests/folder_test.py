import os

import ldiff_test_common as tcommon

tcommon.init()

work_folder = tcommon.create_random_folder()
source_folder = tcommon.create_random_folder(work_folder)
target_folder = tcommon.create_random_folder(work_folder, False)

print(f'source folder : {source_folder}')
print(f'target folder : {target_folder}')

source = tcommon.generate_random_folder(source_folder)
target = tcommon.clone_folder(source, target_folder)

print('\t source folder structure :')
print("-------------------------------------------------")
tcommon.print_folder_record_info(source_folder, source['record'])
print("-------------------------------------------------")

print('\t target folder structure :')
print("-------------------------------------------------")
tcommon.print_folder_record_info(target_folder, target['record'])
print("-------------------------------------------------")

tcommon.random_modify_folder(target)

print('\t modified target folder structure :')
print("-------------------------------------------------")
tcommon.print_folder_record_info(target_folder, target['record'])
print("-------------------------------------------------")

patch_info = tcommon.generate_folder_patch(source_folder, target_folder, parent_folder=work_folder)
print('diff result : ', patch_info)
if patch_info['code'] != 0:
    tcommon.failed(patch_info['code'])

apply_info = tcommon.apply_folder_patch(source_folder, patch_info)
print('apply result : ', apply_info)
if apply_info['code'] != 0:
    tcommon.failed(apply_info['code'])

if apply_info['backupsize'] == 0:
    tcommon.failed(tcommon.ExitCode.BackupFailed)

print('\t applyed source folder structure :')
print("-------------------------------------------------")
tcommon.print_folder_record_info(source_folder, apply_info['applyed']['record'])
print("-------------------------------------------------")

# verify
if not tcommon.cmp_record(target['record'], apply_info['applyed']['record']):
    tcommon.failed(tcommon.ExitCode.ApplyFailed)

# diff again
patch_info = tcommon.generate_folder_patch(source_folder, target_folder, parent_folder=work_folder)
if patch_info['code'] != 2: # 2 -> needless
    tcommon.failed(patch_info['code'])

tcommon.remove_dirs([work_folder])

tcommon.success()