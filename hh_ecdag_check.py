import subprocess
import itertools
import sys

def main(argv):
    if len(argv) != 4:
        print("usage: python hh_ecdag_check.py n k pktsize")
        return

    program = "HHXORTest"
    
    n = int(argv[1])
    k = int(argv[2])
    pktsize = int(argv[3])
    m = n - k

    node_ids = list(range(n))

    for num_failures in range(1, n - k + 1):
        # if num_failures >= 2:
        #     continue
        print("num_failures: {}".format(num_failures))
        for failure_pattern in itertools.combinations(node_ids, num_failures):
            failure_pattern = [str(node_id) for node_id in failure_pattern]
            failed_ids = " ".join(failure_pattern)

            cmd = "./{} {} {}".format(program, pktsize, failed_ids)
            print(cmd)

            p = subprocess.Popen(cmd, shell=True)
            p.wait()

        

if __name__ == "__main__":
    main(sys.argv)