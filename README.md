# cosby

### C-imple
### O-su
### S-tory
### B-oard
### Y-es

# How to run

dependecies needed:
```
qt6-base
qt6-multimedia
qt6-declarative
```

## 1. Clone the repo
    
    git clone https://github.com/<your-username>/cosby.git
    cd cosby
    

## 2. Configure the project
    
    cmake -B build
    

## 3. Build
    
    cmake --build build
    

## 4. Run
    
    ./build/cosby

### Project Structure

```
cosby/
├─ CMakeLists.txt
├─ include/
│  ├─ core/
│  ├─ scripting/
│  ├─ ui/
│  └─ main_window.h
│
├─ src/
│  ├─ core/
│  ├─ scripting/
│  ├─ ui/
│  ├─ main.cpp
│  ├─ main_window.cpp
│  └─ app_actions.cpp
│
├─ scripts/
│  ├─ core/           <---- JS library
|  |   └─ effects/     <---- For some ffects, maybe??
|  |
│  ├─ examples/       <---- just nice to have?
│  └─ types/
│       └─ cosby.d.ts <----  might add for typeScript definitions for monaco??
│
├─ resources/         <---- maybe monaco????
│  ├─ editor/
│  │   ├─ index.html
│  │   ├─ app.js
│  │   ├─ monaco/...
│  │   └─ qwebchannel.js
│  ├─ icons/
│  └─ editor.qrc
│
└─ build/                 # cmake build output
```

<!---
```
cosby/
  include/
  src/
  scripts/        <-----JS library
    core/ 
      effects/     <-----For some ffects, maybe??
```
--->
