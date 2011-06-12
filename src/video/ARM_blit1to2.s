@ ARM code version of Blit1to2.
@
@ @author Robin Watts (robin@wss.co.uk)

	.text

	.global	Blit1to2ARM

	@ Reads a width x height block of 8 bpp pixels from srcPtr, looks
	@ them up in the palette at map, and stores them as 16bpp pixels
	@ at dstPtr. srcPitch and dstPitch give information about how to
	@ find the next lines.
Blit1to2ARM:
	@ r0 = srcPtr
	@ r1 = srcPitch
	@ r2 = dstPtr
	@ r3 = dstPitch
	MOV	r12,r13
	STMFD	r13!,{r4-r11,r14}
	LDMIA	r12,{r4-r6}
	@ r4 = width
	@ r5 = height
	@ r6 = map

	SUBS	r5,r5,#1		@ while (--height)
	BLT	end
height_loop:
	SUBS	r7,r4,#5		@ r7= width_minus_5
	BLE	thin
width_loop:
	@ Have to do at least 6 here
	LDRB	r8, [r0],#1		@ r8 = *src++
	LDRB	r9, [r0],#1		@ r9 = *src++
	LDRB	r10,[r0],#1		@ r10= *src++
	LDRB	r11,[r0],#1		@ r11= *src++
	LDRB	r12,[r0],#1		@ r12= *src++
	LDRB	r14,[r0],#1		@ r14= *src++
	ADD	r8, r8, r8		@ r8 = 2*r8
	ADD	r9, r9, r9		@ r9 = 2*r9
	ADD	r10,r10,r10		@ r10= 2*r10
	ADD	r11,r11,r11		@ r11= 2*r11
	ADD	r12,r12,r12		@ r12= 2*r12
	ADD	r14,r14,r14		@ r14= 2*r14
	LDRH	r8, [r6,r8]		@ r8 = map[r8]
	LDRH	r9, [r6,r9]		@ r9 = map[r9]
	LDRH	r10,[r6,r10]		@ r10= map[r10]
	LDRH	r11,[r6,r11]		@ r11= map[r11]
	LDRH	r12,[r6,r12]		@ r12= map[r12]
	LDRH	r14,[r6,r14]		@ r14= map[r14]
	SUBS	r7,r7,#6		@ r7 = width_minus_5 -= 6
	STRH	r8, [r2],#2		@ *dstPtr++ = r8
	STRH	r9, [r2],#2		@ *dstPtr++ = r9
	STRH	r10,[r2],#2		@ *dstPtr++ = r10
	STRH	r11,[r2],#2		@ *dstPtr++ = r11
	STRH	r12,[r2],#2		@ *dstPtr++ = r12
	STRH	r14,[r2],#2		@ *dstPtr++ = r14
	BGT	width_loop		@ width_minus_5>0 => 6+ left to do
thin:
	ADDS	r7,r7,#5		@ r7 = width (width <= 5)
	BEQ	end_thin
thin_lp:
	LDRB	r8,[r0],#1		@ r8 = *src++
	SUBS	r7,r7,#1		@ r7 = width--
	@ Stall
	ADD	r8,r8,r8		@ r8 = 2*r8
	LDRH	r8,[r6,r8]		@ r8 = map[r8]
	@ Stall
	@ Stall
	STRH	r8,[r2],#2		@ *dstPtr++ = r8
	BGT	thin_lp
end_thin:

	ADD	r2,r2,r3		@ dstPtr += dstPitch
	ADD	r0,r0,r1		@ srcPtr += srcPitch

	SUBS	r5,r5,#1
	BGE	height_loop

end:
	LDMFD	r13!,{r4-r11,PC}
