# How to Run: python3 part1.py addr-simpleloop.ref 
# the program only prints the top 4 pages that have the highest amount of access for both instructions and data

import sys

def analysis(filename):
    # opens the file
    f = open(str(filename), "r")
    ints = 0
    loads = 0
    stores = 0
    modi = 0
    iDict = {}
    dataDict = {}
    for line in f:
        # some data cleaning
        cleanLine = line.strip()
        mode = cleanLine[0]
        address = cleanLine[1:-2].strip()
        page = address[1:-3]

        # if it is Instruction add it to iDict and increase count for I
        if mode == "I":
            ints += 1
            if page in iDict:
                iDict[page] += 1
            else:
                iDict[page] = 1
            continue
        # increase count for other parameters
        elif mode == "L":
            loads += 1
        elif mode == "S":
            stores += 1
        elif mode == "M":
            modi += 1
        # if it is not a Instruction add it to dataDict
        if page in dataDict:
            dataDict[page] += 1
        else:
            dataDict[page] = 1
    
    print("Counts:")
    print("  Instructions {}".format(ints))
    print("  Loads        {}".format(loads))
    print("  Stores       {}".format(stores))
    print("  Modifies     {}\n".format(modi))


    print("Instructions:")
    newIDict = sorted(iDict.items(), key=lambda x: x[1], reverse=True)
    times = 0
    for i in newIDict:
        output = "0x" + i[0] + "000" + "," + str(i[1])
        print(output)
        times += 1
        if times == 4:
            break

    print("Data:")
    newDataDict = sorted(dataDict.items(), key=lambda x: x[1], reverse=True)
    times = 0
    for i in newDataDict:
        output = "0x" + i[0] + "000" + "," + str(i[1])
        print(output)
        times += 1
        if times == 4:
            break




if __name__ == "__main__":
    filename = sys.argv[1]
    analysis(filename)
    