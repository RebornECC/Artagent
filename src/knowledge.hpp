#pragma once
// Профиль техники, выведенный из 19 разобранных уровней (Room Box/3D, GD in 3D, FNaF PLUS 2, The Office Void, Cube Boss).
// Это не обучение весов, а подсказка для шага Plan.
inline constexpr char kStyleProfile[] =
"Target format: the picture is traced into small flat squares (Geometry Dash object 211, the 'pixel' block used in "
"the reference levels: 211k of 218k objects in a big FNaF level, 6.7k of 13.5k in an office level; typical scale about 0.3, "
"i.e. roughly 9 units per pixel). So: crisp pixel-art-like regions, hard edges, NO gradients, NO soft shadows, NO fine noise, "
"large readable color areas, a palette of at most 8 colors with strong contrast, objects readable at small size. "
"Reference levels build 3D-looking rooms and characters from boxes with clear light/mid/dark faces (three tones per surface), "
"so prefer a simple isometric or front view with consistent light from the top-left.";
