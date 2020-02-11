fileList = ["callback_1.txt", "callback_2.txt", "callback_3.txt", "callback_4.txt", "callback_5.txt"]

for file in fileList:
    with open(file, "r") as f:
        first = 0
        avg = 0
        for line in f:
           res = line.split(" ", 1)
           if res[1].isdigit():
               time = res[1]
               if first == 0:
                   first = 1
                   avg = time
               else:
                   avg = (avg + time) / 2
        f.close()
        print(avg)
