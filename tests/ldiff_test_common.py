import os
import sys
import shutil
import random
import string
import hashlib
import copy

from enum import Enum

WORK_DIR = 'ldiff_tests_temp'
CHINESE_DICT = [] # ['中', '文', '汉', '字', '龙', '年', '大', '吉', '利']
RANDOM_STRING_SEED = list(string.ascii_letters + string.digits) + CHINESE_DICT
MIN_FILE_SIZE = 0                 # 512 Bytes
MAX_FILE_SIZE = 10 * (1024 << 10) # 10 MB


class ExitCode(Enum):
    Success      = 0
    ApplyFailed  = 100
    BackupFailed = 101


def init():
    try:
        if not os.path.exists(WORK_DIR):
            os.makedirs(WORK_DIR)
    except:
        pass

def deinit():
    # if os.path.exists(WORK_DIR):
    #         shutil.rmtree(WORK_DIR)
    pass

def complete(code):
    deinit()

    if isinstance(code, ExitCode):
        sys.exit(code.value)
    else:
        sys.exit(code)

def success():
    complete(0)

def failed(code):
    complete(code)

def remove_dirs(dirs):
    for dir in dirs:
        if os.path.exists(dir):
            shutil.rmtree(dir)

def random_string(l):
    return ''.join(random.sample(RANDOM_STRING_SEED, min(l, len(RANDOM_STRING_SEED))))

def random_name():
    return random_string(10)

def random_filename():
    return random_string(10) + '.bin'

def random_bytes(l):
    return bytearray(os.urandom(l))

def random_filesize(min_size = MIN_FILE_SIZE, max_size = MAX_FILE_SIZE):
    return random.randint(min_size, max_size)

def content_hash(data):
    hasher = hashlib.sha256()
    hasher.update(data)
    return hasher.hexdigest()

def file_hash(filepath):
    hasher = hashlib.sha256()
    
    with open(filepath, 'rb') as file:
        hasher.update(file.read())

    return hasher.hexdigest()

def generate_random_file(filename = '', parent = WORK_DIR, filesize = 0, sub_folder=True):
    if len(filename) == 0:
        filename = random_filename()

    folder = parent

    if sub_folder:
        folder = os.path.join(folder, random_string(12))

    if filesize == 0:
        filesize = random_filesize()

    filepath = os.path.join(folder, filename)
    content = random_bytes(filesize)
    filehash = content_hash(content)

    if not os.path.exists(folder):
        os.makedirs(folder)
    
    with open(filepath, 'wb') as file:
        file.write(content)
    
    return {
        'filename': filename,
        'filepath': filepath,
        'filehash': filehash,
        'filesize': filesize,
    }

def create_random_folder(parent = WORK_DIR, create = True):
    folder = os.path.join(parent, random_string(12))
    if create and not os.path.exists(folder):
        os.makedirs(folder)
    return folder

def print_folder_record_info(folder_path, record):
    print(os.path.join(folder_path, record['folder']))
    for f in record['files']:
        file_path = os.path.join(folder_path, f['filepath'])
        file_size = f['filesize']
        print(f'{file_path}[{file_size}]')
    for folder in record['sub_folders']:
        print_folder_record_info(folder_path, folder)

def _generate_folder_record(path):
    return {
        'folder': path,
        'files': [],
        'sub_folders': []
    }

def _regular_folder_record(record, source_folder):
    strip_len = len(source_folder) + 1
    record['folder'] = record['folder'][strip_len:]
    for file in record['files']:
        file['filepath'] = file['filepath'][strip_len:]
    for sub_folder in record['sub_folders']:
        _regular_folder_record(sub_folder, source_folder)

def _do_generate_random_folder(record, times):
    if times <= 0:
        return
    
    record['files'] = [
        generate_random_file(parent=record['folder'], filesize=random_filesize(MIN_FILE_SIZE, 2 * (1024 << 10)), sub_folder=False),
        generate_random_file(parent=record['folder'], filesize=random_filesize(MIN_FILE_SIZE, 2 * (1024 << 10)), sub_folder=False),
        generate_random_file(parent=record['folder'], filesize=random_filesize(MIN_FILE_SIZE, 2 * (1024 << 10)), sub_folder=False),
    ]

    record['sub_folders'] = [
        _generate_folder_record(create_random_folder(parent=record['folder'])),
        _generate_folder_record(create_random_folder(parent=record['folder'])),
        _generate_folder_record(create_random_folder(parent=record['folder'])),
    ]

    for sub_folder in record['sub_folders']:
        _do_generate_random_folder(sub_folder, times - 1)

def generate_random_folder(folder):
    record = _generate_folder_record(folder)
    _do_generate_random_folder(record, 2)
    _regular_folder_record(record, folder)

    return {
        'folderpath': folder,
        'record': record,
    }

def clone_folder(source_info, target_folder):
    source_folder = source_info['folderpath']
    shutil.copytree(source_folder, target_folder)

    return {
        'folderpath': target_folder,
        'record': copy.deepcopy(source_info['record'])
    }

def _random_modify_file(target_folder, file_info):
    file_path = os.path.join(target_folder, file_info['filepath'])
    
    mode = random.randint(0, 4)
    
    if mode in [0, 4]:
        # 0 -> append to tail
        # 4 -> new file
        rand_data = random_bytes(random_filesize(512, 1 * (1024 << 10)))
        with open(file_path, 'ab+' if mode == 0 else 'wb') as file:
            file.write(rand_data)
        file_info['filehash'] = file_hash(file_path)
        file_info['filesize'] = os.path.getsize(file_path)
        return
    
    with open(file_path, 'rb') as file:
        content = file.read()

    if mode == 1:
        # insert to head
        content = random_bytes(random_filesize(512, 1 * (1024 << 10))) + content
    elif mode == 2:
        # insert to rand pos
        pos = random.randint(0, len(content))
        content = content[:pos] + random_bytes(random_filesize(512, 1 * (1024 << 10))) + content[pos:]
    elif mode == 3:
        # truncate
        lpos = random.randint(0, len(content))
        rpos = lpos + random.randint(0, len(content) - lpos)
        content = content[:lpos] + content[rpos:]
    
    file_info['filehash'] = content_hash(content)
    file_info['filesize'] = len(content)
    
    with open(file_path, 'wb') as file:
        file.write(content)

def _do_random_modify_folder(target_folder, record, times):
    folder_path = os.path.join(target_folder, record['folder'])
    
    # modify second file
    _random_modify_file(target_folder, record['files'][1])
    
    # remove one file and folder
    removed_file = os.path.join(target_folder, record['files'].pop()['filepath'])
    removed_sub_folder = os.path.join(target_folder, record['sub_folders'].pop()['folder'])
    
    if os.path.exists(removed_file):
        os.remove(removed_file)
    if os.path.exists(removed_sub_folder):
        shutil.rmtree(removed_sub_folder)
    
    # add new file
    record['files'].append(
        generate_random_file(parent=folder_path, filesize=random_filesize(MIN_FILE_SIZE, 2 * (1024 << 10)), sub_folder=False)
    )
    new_file = record['files'][len(record['files']) - 1]
    new_file['filepath'] = new_file['filepath'][len(target_folder) + 1:]
    
    # add new folder
    new_folder_record = _generate_folder_record(create_random_folder(parent=folder_path))
    _do_generate_random_folder(new_folder_record, 1)
    _regular_folder_record(new_folder_record, target_folder)
    record['sub_folders'].append(new_folder_record)
    
    if times <= 0:
        return
    
    # modify second folder
    _do_random_modify_folder(target_folder, record['sub_folders'][1], times - 1)

def random_modify_folder(target_info):
    _do_random_modify_folder(target_info['folderpath'], target_info['record'], 1)
    return target_info

def _do_scan_applyed_folder(record):
    files = []
    sub_folders = []
    
    for item in os.scandir(record['folder']):
        path = os.path.join(record['folder'], item.name)
        if item.is_dir():
           sub_folders.append(_generate_folder_record(path))
        else:
            files.append({
                'filename': item.name,
                'filepath': path,
                'filehash': file_hash(path),
                'filesize': os.path.getsize(path)
            })
    
    record['files'] = files
    record['sub_folders'] = sub_folders
    
    for sub_folder in record['sub_folders']:
        _do_scan_applyed_folder(sub_folder) 

def scan_applyed_folder(folder):
    record = _generate_folder_record(folder)
    _do_scan_applyed_folder(record)
    _regular_folder_record(record, folder)
    
    return {
        'folderpath': folder,
        'record': record,
    }

def _do_collect_info(record, result):
    result['files'].extend(record['files'])
    for sub_folder in record['sub_folders']:
        result['sub_folders'].append(sub_folder['folder'])
        _do_collect_info(sub_folder, result)

def _collect_info(record):
    result = {
        'files': [],
        'sub_folders': []
    }
    _do_collect_info(record, result)
    return {
        'files': {frozenset(f.items()) for f in result['files']},
        'sub_folders': set(result['sub_folders'])
    }

def cmp_record(record1, record2):
    record_info1 = _collect_info(record1)
    record_info2 = _collect_info(record2)
    
    file_diff = record_info1['files'] ^ record_info2['files']
    folder_diff = record_info1['sub_folders'] ^ record_info2['sub_folders']
    
    print('file diff : ')
    print(file_diff)
    print('folder diff : ')
    print(folder_diff)
    
    return len(file_diff) == 0 and len(folder_diff) == 0

def generate_patch(source, target, fpe = False, parent_folder = WORK_DIR):
    source_path = source['filepath']
    target_path = target['filepath']
    patch_folder = os.path.join(parent_folder, source['filename'] + '-patch')
    patch_path = os.path.join(patch_folder, source['filename'])
    
    cmd = f'ldiff diff -f \"{source_path}\" \"{target_path}\" \"{patch_path}\" {"-e" if fpe else ""}'
    print(f'generate patch cmd : {cmd}')

    return {
        'patchfolder': patch_folder,
        'patchpath': patch_path + ('.exe' if fpe else '.lpatch'),
        'code': os.system(cmd)
    }

def apply_patch(source, patch, fpe = False):
    source_path = source['filepath']
    patch_path = patch['patchpath']
    patch_folder = patch['patchfolder']
    backupname = random_string(10) + '.ldiffbak.7z'
    backuppath = os.path.join(patch_folder, backupname)
    
    if fpe:
        cmd = f'.\\\"{patch_path}\" \"{source_path}\" --backup \"{backuppath}\"'
    else:
        cmd = f'ldiff patch \"{source_path}\" \"{patch_path}\" --backup \"{backuppath}\"' 

    print(f'apply patch cmd : {cmd}')

    code = os.system(cmd)
    if code != 0:
        return {
            'code': code
        }
    
    return {
        'code': code,
        'filehash': file_hash(source_path),
        'backupsize': os.path.getsize(backuppath) if os.path.exists(backuppath) else 0
    }

def generate_pe_patch(source, target, parent_folder = WORK_DIR):
    return generate_patch(source, target, True, parent_folder=parent_folder)

def apply_pe_patch(source, patch):
    return apply_patch(source, patch, True)

def generate_folder_patch(source_path, target_path, fpe = False, parent_folder = WORK_DIR):
    patch_folder = os.path.join(parent_folder, os.path.basename(source_path) + '-patch')
    patch_path = os.path.join(patch_folder, os.path.basename(source_path))
    
    cmd = f'ldiff diff -f \"{source_path}\" \"{target_path}\" \"{patch_path}\" {"-e" if fpe else ""}'
    print(f'generate patch cmd : {cmd}')

    return {
        'patchfolder': patch_folder,
        'patchpath': patch_path + ('.exe' if fpe else '.lpatch'),
        'code': os.system(cmd)
    }

def apply_folder_patch(source_path, patch, fpe = False):
    patch_path = patch['patchpath']
    patch_folder = patch['patchfolder']
    backupname = random_string(10) + '.ldiffbak.7z'
    backuppath = os.path.join(patch_folder, backupname)
    
    if fpe:
        cmd = f'.\\\"{patch_path}\" \"{source_path}\" --backup \"{backuppath}\"'
    else:
        cmd = f'ldiff patch \"{source_path}\" \"{patch_path}\" --backup \"{backuppath}\"' 

    print(f'apply patch cmd : {cmd}')

    code = os.system(cmd)
    if code != 0:
        return {
            'code': code
        }
    
    return {
        'code': code,
        'backupsize': os.path.getsize(backuppath) if os.path.exists(backuppath) else 0,
        'applyed': scan_applyed_folder(source_path)
    }

def generate_pe_folder_patch(source_path, target_path, parent_folder = WORK_DIR):
    return generate_folder_patch(source_path, target_path, True, parent_folder=parent_folder)

def apply_pe_folder_patch(source_path, target_path):
    return apply_folder_patch(source_path, target_path, True)