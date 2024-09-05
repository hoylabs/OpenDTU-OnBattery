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

def check_files(directories, filepaths, hash_file):
    old_file_hashes = {}
    file_hashes = {}

    for directory in directories:
        for root, dirs, filenames in os.walk(directory):
            for file in filenames:
                filepaths.append(os.path.join(root, file))

    for file_path in filepaths:
        file_hashes[file_path] = calculate_hash(file_path)

    if os.path.exists(hash_file):
        with open(hash_file, 'rb') as f:
            old_file_hashes = pickle.load(f)

    for file_path, file_hash in file_hashes.items():
        if file_path not in old_file_hashes or old_file_hashes[file_path] != file_hash:
            print("compiling webapp (hang on, this can take a while and there might be little output)...")

            try:
                # if these commands fail, an exception will prevent us from
                # persisting the current hashes => commands will be executed again
                subprocess.run(["yarn", "--cwd", "webapp", "install", "--frozen-lockfile"],
                               check=True)

                subprocess.run(["yarn", "--cwd", "webapp", "build"], check=True)

            except FileNotFoundError:
                raise Exception("it seems 'yarn' is not installed/available on your system")

            with open(hash_file, 'wb') as f:
                pickle.dump(file_hashes, f)

            return

    print("webapp artifacts should be up-to-date")

def main():
    if os.getenv('GITHUB_ACTIONS') == 'true':
        print("INFO: not testing for up-to-date webapp artifacts when running as Github action")
        return 0

    print("INFO: testing for up-to-date webapp artifacts")

    directories = ["webapp/src/", "webapp/public/"]
    files = ["webapp/index.html", "webapp/tsconfig.config.json",
             "webapp/tsconfig.json", "webapp/vite.config.ts",
             "webapp/yarn.lock"]
    hash_file = "webapp_dist/.hashes.pkl"

    check_files(directories, files, hash_file)

main()
