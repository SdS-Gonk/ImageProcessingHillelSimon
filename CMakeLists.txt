cmake_minimum_required(VERSION 3.10)

# Définir le nom du projet et le langage
project(ImageProcessingHillelSimon C)

# Définir le standard C (C99 ou C11 sont de bons choix modernes)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Utile pour certains outils d'analyse statique et IDEs (ex: clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ajouter la cible exécutable
# CMake trouvera automatiquement les fichiers d'en-tête (.h) correspondants
# s'ils sont dans le même répertoire que les fichiers source (.c)
add_executable(image_processor
    main.c
    bmp8.c
    bmp24.c
    histogram.c
)

# Lier la bibliothèque mathématique (libm)
# Nécessaire pour des fonctions comme round(), roundf(), abs() dans math.h
# target_link_libraries gère la portabilité (ex: pas besoin de -lm avec MSVC)
target_link_libraries(image_processor PRIVATE m)

# Optional: Add include directories if headers were in a separate 'include' folder
# target_include_directories(image_processor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Optionnel : Ajouter des options de compilation pour les avertissements
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # Options pour GCC et Clang
  target_compile_options(image_processor PRIVATE -Wall -Wextra -pedantic)
elif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  # Options pour Microsoft Visual C++
  target_compile_options(image_processor PRIVATE /W4)
endif()

# Optionnel : Définir le répertoire de sortie pour l'exécutable (par défaut, c'est la racine du dossier de build)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Afficher un message à la fin de la configuration
message(STATUS "Configuration CMake terminée pour ${PROJECT_NAME}.")
message(STATUS "Répertoire de build : ${CMAKE_BINARY_DIR}")
message(STATUS "Pour compiler, utilisez : cmake --build ${CMAKE_BINARY_DIR}")
