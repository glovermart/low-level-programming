.section .text  

.global _start

_start:

LDR  R0, =input             //Load base address of input string

check_input:                
LDRB R1, [R0]          		//Load a byte(immediate character) from the input string at address R0 and store in R1
CMP  R1, #0					//Compare the ascii value to the null character (0) i.e end of string				
BEQ  check_palindrome   	//Upon detecting end of string, branch to check_palindrome label to check if string is a palindrome
ADD  R2, R2, #1         	//For valid characters detected, keep a count - increment by 1.
ADD  R0, R0, #1         	//Go to the next byte in memory
B    check_input            //Loop back
		
check_palindrome: 
CMP  R2, #2
BLT  palindrome_not_found	//Specification requires palindrome to be a minium of two characters. Dsisplay palindrome not found
LDR  R3, =input				//Load base address of input string again - R0 is dirty - value modified     
ADD  R4, R2,R3 				//Manipulate string , add offfset(string length)
SUB  R4, R4,#1				//Go to the last byte of the input string
MOV  R2, #0                 //clear register R2 to avoid spurious output for new runs(restart + continue) in CPUlator   
                            //R2 holds old value from previous iteration and must be cleared before checking length of input string

string_compare_front:       //Gets a character from the string - Front/LHS - Reference: R3
LDRB R1, [R3]               //Load a byte('immediate' character) from the input string at address R3 and store in R1
CMP  R1, #0x20              //Compare the character to to ascii value of 'space' =32 in dec
BEQ  next_char_from_front   //Branch to next_char_from_front label if space is detected and get next byte/character - Ignores spaces in string
B	 string_compare_back    //Now check character from the other end of string  
next_char_from_front: 
ADD  R3, R3,#1   
B    string_compare_front	

string_compare_back:        //Gets a character from the string - Back/RHS - Reference: R4
LDRB R0, [R4]     
CMP  R0, #0x20
BEQ  next_char_from_back    //Branch to next_char_from_back label if space is detected and get next byte/character - Ignores spaces in string         
B	 string_compare
next_char_from_back: 
SUB  R4, R4,#1  
B    string_compare_back
       

string_compare:             //Checks characters from 'equidistant' locations in the string,compares ascii value and checks 'original' string and 'flipped' string
CMP  R1, R0        
BNE  check_case_            //If characters aren't equal in AsCII value, check if other character is upper case or lowercase
ADD  R3, R3,#1      
SUB  R4, R4,#1     
CMP  R3, R4         
BGE  palindrome_found
B    string_compare_front


check_case_:                //Checks if the other character being compared is the lower or upper case of the other
CMP  R1, R0      
BGT  check_uppercase	    //R0 could be a lower case, check if R1 is upper case of R0 by adding numberic value 32
ADD  R1, R1, #0x20          //Add 32 to R1 and compare if R1 is less than R0
CMP  R1, R0					
BEQ  string_compare         //If characters are the same, go to string_compare  and check the both strings
B    palindrome_not_found

check_uppercase: 
ADD  R0, R0, #0x20
CMP  R1, R0
BEQ  string_compare         //If characters are the same, go to string_compare  and check the both strings
B    palindrome_not_found

palindrome_found:           //Turns on the five rightmost LEDS and continues to write to JTAG UART
LDR  R5, =0xff200000        //Go to Data register of LEDS
LDR  R6, =0x0000001f
STR  R6, [R5]               //write to register - mask bits with 0x1f(R6)
LDR  R7, =0xff201000        //Go to Data register of JTAG UART (BASE ADDRESS)
LDR  R8, =output_PF         

write_UART_PF:              //Loop- writes a byte to UART at a time, exits(clears 'register of concern' prior) when the null character is encountered
// LDRB R9, [R8],#1 //Skips first byte - not useful in this case
LDRB R9, [R8]    
CMP  R9, #0          
BEQ  exit       
STR  R9, [R7]               //Write character to JTAG UART
ADD  R8, R8, #1      
B    write_UART_PF
				  
palindrome_not_found:       //Turns on the five leftmost LEDS and continues to write to JTAG UART
MOV  R2, #0                 //clear register R2 to avoid spurious output for new runs(restart + continue)   
                            //R2 holds old value from previous iteration and must be cleared before checking length of input string
LDR  R5, =0xff200000
LDR  R6, =0x000003e0 
STR  R6, [R5]				//write to register - mask bits with 0x3E0(R6) 
LDR  R7, =0xff201000
LDR  R8, =output_PnF

write_UART_PnF:             //Loop- writes a byte to UART at a time, exits(clears 'register of concern' prior) when the null character is encountered
// LDRB R9, [R8],#1 //Skips first byte - not useful in this case
LDRB R9, [R8]    
CMP  R9, #0
BEQ  exit   
STR  R9, [R7]    
ADD  R8, R8, #1  
B    write_UART_PnF					  
					  
exit:
B     exit

.section .data  
	
.align
	 
input: .asciz "level"
	// input: .asciz "8448"
    // input: .asciz "KayAk"
    // input: .asciz "step on no pets"
    // input: .asciz "Never odd or even"

output_PF: 
.string "Palindrome detected\n"

output_PnF: 
.string "Not a palindrome\n"

.end