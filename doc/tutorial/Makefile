# Needs pandoc to be installed. 
all: pandoc.css gdp-tutorial-part1.md gdp-tutorial-part2.md
	pandoc -s -S --toc --toc-depth=5 -c pandoc.css gdp-tutorial-part1.md -o gdp-tutorial-part1.html
	# pandoc -s -S --toc --toc-depth=5 -c pandoc.css gdp-tutorial-part1.md -o gdp-tutorial-part1.pdf
	pandoc -s -S --toc --toc-depth=5 -c pandoc.css gdp-tutorial-part2.md -o gdp-tutorial-part2.html
	# pandoc -s -S --toc --toc-depth=5 -c pandoc.css gdp-tutorial-part2.md -o gdp-tutorial-part2.pdf
