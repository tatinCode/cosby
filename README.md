# cosby

### C-imple
### O-su
### S-tory
### B-oard
### Y-es

# How to run

# 1. Clone the repo
git clone https://github.com/<your-username>/cosby.git
cd cosby

# 2. Configure the project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build -j$(nproc)

# 4. Run
./build/cosby