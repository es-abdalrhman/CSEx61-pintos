#define F (1 << 14) // 17.14 fixed-point 
// 1 -> 16384 in this case 
#define INT_TO_FP(n) ((n) * F)
#define FP_TO_INT_ZERO(x) ((x) / F)
#define FP_TO_INT_NEAR(x) ((x) >= 0 ? ((x) + F / 2) / F : ((x) - F / 2) / F)
#define ADD_FP(x, y) ((x) + (y))
#define SUB_FP(x, y) ((x) - (y))
#define MUL_FP(x, y) ((int64_t)(x)) * (y) / F // int64_t to avoid overflow , x ,y are Xvalue,Yvalue both multiplied by F so we divide by F to get the correct fixed point repesentation for the result
#define DIV_FP(x, y) ((int64_t)(x)) * F / (y) // for the division F multipled by X would be canceled by Y one so we multiply by to keep fixed point repesentation