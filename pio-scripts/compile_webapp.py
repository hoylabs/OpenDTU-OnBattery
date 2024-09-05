import os
import hashlib
import pickle
import subprocess

# Funktion zur Berechnung des Hash-Werts einer Datei
def calculate_hash(file_path):
    hasher = hashlib.md5()
    with open(file_path, 'rb') as f:
        buf = f.read()
        hasher.update(buf)
    return hasher.hexdigest()

# Funktion zur Überprüfung der Dateien in einem Ordner
def check_files(directory, hash_file):
    file_hashes = {}

    # Alle Dateien im Ordner und Unterordner durchsuchen
    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            file_hashes[file_path] = calculate_hash(file_path)

    # Laden der vorherigen Hash-Werte
    if os.path.exists(hash_file):
        with open(hash_file, 'rb') as f:
            old_file_hashes = pickle.load(f)
    else:
        old_file_hashes = {}

    changed = False
    # Vergleich der aktuellen Hash-Werte mit den vorherigen
    for file_path, file_hash in file_hashes.items():
        if file_path not in old_file_hashes or old_file_hashes[file_path] != file_hash:
            changed = True
            break

    if not changed:
        print("No changes detected.")
    else:
        print(f"webapp changed.")
        execute_command(directory)

    # Speichern der aktuellen Hash-Werte
    with open(hash_file, 'wb') as f:
        pickle.dump(file_hashes, f)

# Funktion zur Ausführung eines Befehls
def execute_command(directory):
    print("Webapp changed, rebuilding...")
    # change working directory
    print(f"Changing directory to: {directory}")
    os.chdir(directory)
    # Run yarn install
    result = subprocess.run(["yarn", "install"], shell=True)
    if result.returncode != 0:
        print("Error during yarn install.")
        return
    # Run yarn build
    result = subprocess.run(["yarn", "build"], shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print("Error during yarn build:")
        print(result.stdout)
        print(result.stderr)
    else:
        print("Build completed successfully.")
    # move back to old directory
    os.chdir("..")

# Pfad zum Ordner und zur Hash-Datei
directory = 'webapp'
hash_file = ".webapp_hashes.pkl"

# Überprüfung der Dateien
print("checke webapp")
check_files(directory, hash_file)
