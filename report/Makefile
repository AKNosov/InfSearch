filename=M8O-406Б-22_Носов_Александр_Константинович

pdf:
	pdflatex ${filename}.tex
	pdflatex ${filename}.tex

clean:
	rm -f ${filename}.{ps,pdf,log,aux,out,dvi,bbl,blg,toc,lof,lot}

view: pdf
	open ${filename}.pdf  # macOS
	# xdg-open ${filename}.pdf  # Linux

.PHONY: pdf clean view