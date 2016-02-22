#ifndef FIXED_NUM_H
#define FIXED_NUM_H

#define SHIFT_AMOUNT 14

#define CONVERT(A) ((int)(A << SHIFT_AMOUNT))
/* Add two fixed-point value. */
#define ADD(A,B) (A + B)
/* Add a fixed-point value A and an int value B. */
#define ADD_MIX(A,B) (A + (B << SHIFT_AMOUNT))
/* Substract two fixed-point value. */
#define SUB(A,B) (A - B)
 /* Substract an int value B from a fixed-point value A */
#define SUB_MIX(A,B) (A - (B << SHIFT_AMOUNT))
/* Multiply a fixed-point value A by an int value B. */
#define MULT_MIX(A,B) (A * B)
/* Divide a fixed-point value A by an int value B. */
#define DIV_MIX(A,B) (A / B)
/* Multiply two fixed-point value. */
#define MULT(A,B) ((int)(((int64_t) A) * B >> SHIFT_AMOUNT))		
/* Divide two fixed-point value. */
#define DIV(A,B) ((int)((((int64_t) A) << SHIFT_AMOUNT) / B))
/* Get integer part of a fixed-point value. */
#define INT_PART(A) (A >> SHIFT_AMOUNT)
/* Get rounded integer of a fixed-point value. */
#define ROUND(A) (A >= 0 ? ((A + (1 << (SHIFT_AMOUNT - 1))) >> SHIFT_AMOUNT) : ((A - (1 << (SHIFT_AMOUNT - 1))) >> SHIFT_AMOUNT))
		  
#endif 
