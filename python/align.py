import random

# Memoization dictionary
memo = {}

def findOptimalAlignment(strand1, strand2):
    # Check if the result is already cached
    if (strand1, strand2) in memo:
        return memo[(strand1, strand2)]
    
    # Base case: if one of the two strands is empty
    if len(strand1) == 0:
        result = (len(strand2) * -2, ' ' * len(strand2), strand2)
    elif len(strand2) == 0:
        result = (len(strand1) * -2, strand1, ' ' * len(strand1))
    else:
        # Initialize the score and alignments
        score = aligned1 = aligned2 = None
        
        # Calculate the score and alignments
        if strand1[0] == strand2[0]:
            score, aligned1, aligned2 = findOptimalAlignment(strand1[1:], strand2[1:])
            score += 1
            aligned1 = strand1[0] + aligned1
            aligned2 = strand2[0] + aligned2
        else:
            # Scenario 1: insert space in strand1
            score1, aligned1_1, aligned2_1 = findOptimalAlignment(strand1[1:], strand2)
            score1 -= 2
            aligned1_1 = strand1[0] + aligned1_1
            aligned2_1 = ' ' + aligned2_1

            # Scenario 2: insert space in strand2
            score2, aligned1_2, aligned2_2 = findOptimalAlignment(strand1, strand2[1:])
            score2 -= 2
            aligned1_2 = ' ' + aligned1_2
            aligned2_2 = strand2[0] + aligned2_2

            # Scenario 3: mismatch
            score3, aligned1_3, aligned2_3 = findOptimalAlignment(strand1[1:], strand2[1:])
            score3 -= 1
            aligned1_3 = strand1[0] + aligned1_3
            aligned2_3 = strand2[0] + aligned2_3

            # Choose the best score
            if score1 >= score2 and score1 >= score3:
                score, aligned1, aligned2 = score1, aligned1_1, aligned2_1
            elif score2 >= score3:
                score, aligned1, aligned2 = score2, aligned1_2, aligned2_2
            else:
                score, aligned1, aligned2 = score3, aligned1_3, aligned2_3

        result = (score, aligned1, aligned2)

    # Cache the result
    memo[(strand1, strand2)] = result
    return result

# Utility function that generates a random DNA string of a random length drawn from the range [minlength, maxlength]
def generateRandomDNAStrand(minlength, maxlength):
    assert minlength > 0, "Minimum length passed to generateRandomDNAStrand must be a positive number"
    assert maxlength >= minlength, "Maximum length passed to generateRandomDNAStrand must be at least as large as the specified minimum length"
    
    strand = ""
    length = random.choice(range(minlength, maxlength + 1))
    bases = ['A', 'T', 'G', 'C']
    for i in range(0, length):
        strand += random.choice(bases)
    return strand

# Example usage
strand1 = generateRandomDNAStrand(5, 12)
strand2 = generateRandomDNAStrand(5, 12)

print(f"Aligning these two strands: \n{strand1}\n{strand2}")

score, aligned_strand1, aligned_strand2 = findOptimalAlignment(strand1, strand2)

print(f"\nOptimal alignment score is {score}")
print(f"{aligned_strand1}\n{aligned_strand2}")

 
def main():
	while (True):
		sys.stdout.write("Generate random DNA strands? ")
		answer = sys.stdin.readline()
		if answer == "no\n": break
		strand1 = generateRandomDNAStrand(8, 10)
		strand2 = generateRandomDNAStrand(8, 10)
		sys.stdout.write("Aligning these two strands: " + strand1 + "\n")
		sys.stdout.write("                            " + strand2 + "\n")
		alignment = findOptimalAlignment(strand1, strand2)
		printAlignment(alignment)
		
if __name__ == "__main__":
  main()
