import os
import hashlib
import pickle
import subprocess

def calculate_hash(file_path):
    hasher = hashlib.md5()
    with open(file_path, 'rb') as f:
        buf = f.read()
        hasher.update(buf)
    return hasher.hexdigest()

def do_compile(directory):
    print("Webapp changed, rebuilding...")
    print(f"Changing directory to: {directory}")
    os.chdir(directory)
    result = subprocess.run(["yarn", "install"], shell=True)
    if result.returncode != 0:
        print("Error during yarn install.")
        return
    result = subprocess.run(["yarn", "build"], shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print("Error during yarn build:")
        print(result.stdout)
        print(result.stderr)
    else:
        print("Build completed successfully.")
    os.chdir("..")

def check_files(directory, hash_file):
    file_hashes = {}

    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            file_hashes[file_path] = calculate_hash(file_path)

    if os.path.exists(hash_file):
        with open(hash_file, 'rb') as f:
            old_file_hashes = pickle.load(f)
    else:
        old_file_hashes = {}

    changed = False
    for file_path, file_hash in file_hashes.items():
        if file_path not in old_file_hashes or old_file_hashes[file_path] != file_hash:
            changed = True
            break

    if not changed:
        print("No changes detected.")
    else:
        print(f"webapp changed.")
        do_compile(directory)

    with open(hash_file, 'wb') as f:
        pickle.dump(file_hashes, f)

def main():
    if os.getenv('GITHUB_ACTIONS') != 'true':
        print("INFO: not testing for up-to-date webapp artifacts when running as Github action")
        return 0

    directory = 'webapp'
    hash_file = ".webapp_hashes.pkl"

    print("checke webapp")
    check_files(directory, hash_file)

if __name__ == '__main__':
    main()
