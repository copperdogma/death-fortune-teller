Here’s how to correctly convert your Photoshop logo into the required 1-bit BMP (≤384 px wide) that meets your firmware’s printer_logo spec:

⸻

Step-by-Step Conversion in Photoshop
	1.	Open your logo file
Open the PSD or any source version of your logo.
	2.	Flatten the image
	•	Go to Layer → Flatten Image.
	•	The output must be a single layer—no transparency, no alpha.
	3.	Convert to grayscale
	•	Go to Image → Mode → Grayscale.
	•	If Photoshop asks to discard color information, click Discard.
	4.	Resize to ≤ 384 px width
	•	Go to Image → Image Size.
	•	Set Width = 384 pixels (or less).
	•	Make sure Resample = Bicubic Sharper (for reduction).
	5.	Convert to 1-bit (black & white)
	•	Go to Image → Mode → Bitmap.
	•	In the dialog:
	•	Output: 300 ppi (doesn’t matter for the printer, but required field).
	•	Method: 50% Threshold (not halftone or diffusion dither).
	•	This forces a pure 1-bit (black or white only) image.
	6.	Ensure black = background (index 0)
The printer expects index 0 = black, index 1 = white, though firmware inverts if needed.
	•	If your image shows white background and black artwork, you’re good.
	•	If inverted, go to Image → Adjustments → Invert (⌘I) before exporting.
	7.	Export as BMP
	•	Go to File → Save As → Format: BMP.
	•	Click Save, and in the BMP options dialog:
	•	Depth: 1 Bit.
	•	Flip Row Order: Off.
	•	Compress (RLE): None.
	•	Save as logo_384w.bmp.
	8.	Place on SD card
Copy the file to the SD card under:

/printer/logo_384w.bmp

or specify another path in config.txt using:

printer_logo=/printer/mylogo.bmp



⸻

Verification

You can double-check the file:
	•	File size should be roughly ((width × height) / 8) + ~62 bytes header.
	•	Open it in a hex viewer—look for "BM" at start and 01 00 for bit depth.
	•	Or reopen in Photoshop: Image → Mode should show “Bitmap”.

⸻

Would you like a small Python or CLI script to verify bit depth and width before deploying (handy sanity check for the SD bundle)?