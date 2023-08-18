BASE=$(pwd)/../../
DIRPREFIX=./
INSTATDIR=${BASE}/simresults
OUTXMLDIR=${DIRPREFIX}/../mcpat_xml
OUTSTATDIR=${DIRPREFIX}/../mcpat_stat
MCPATDIR=${BASE}/../mcpat/

# Create results folder if it does not exist
mkdir -p $OUTXMLDIR
mkdir -p $OUTSTATDIR

for BENCH in bwaves gcc mcf deepsjeng leela exchange2 xz wrf cam4 pop2 imagick nab fotonik3d roms perlbench x264 xalancbmk omnetpp cactuBSSN lbm
do
    echo "${BENCH}_1_3GHz_classic_statsno.txt"
    python3 ${DIRPREFIX}/mcpat_stat.py --statFile ${INSTATDIR}/${BENCH}_1_3GHz_classic_statsno.txt --templateFile ${DIRPREFIX}/ARM_baseline_3GHz_X2.xml --numCores 1 > ${OUTXMLDIR}/${BENCH}_baseline_1_3GHz_classic.xml # && \
    ${MCPATDIR}/mcpat -infile ${OUTXMLDIR}/${BENCH}_baseline_1_3GHz_classic.xml -print_level 1 > ${OUTSTATDIR}/${BENCH}_baseline_1_3GHz_classic.txt
    for FREQ in 3000MHz 2700MHz 
    do
        echo "${BENCH}_paramedic_1_1_3GHz_${FREQ}_X2_classic_stats.txt"
        python3 ${DIRPREFIX}/mcpat_stat.py --statFile ${INSTATDIR}/${BENCH}_paramedic_1_1_3GHz_${FREQ}_X2_classic_stats.txt --templateFile ${DIRPREFIX}/ARM_1_1_3GHz_${FREQ}_X2.xml --numCores 2 > ${OUTXMLDIR}/${BENCH}_paramedic_1_1_3GHz_${FREQ}_X2_classic.xml # && \
        ${MCPATDIR}/mcpat -infile ${OUTXMLDIR}/${BENCH}_paramedic_1_1_3GHz_${FREQ}_X2_classic.xml -print_level 1 > ${OUTSTATDIR}/${BENCH}_paramedic_1_1_3GHz_${FREQ}_X2_classic.txt
    done
    for FREQ in 1500MHz 1350MHz 
    do
        echo "${BENCH}_paramedic_1_2_3GHz_${FREQ}_X2_classic_stats.txt"
        python3 ${DIRPREFIX}/mcpat_stat.py --statFile ${INSTATDIR}/${BENCH}_paramedic_1_2_3GHz_${FREQ}_X2_classic_stats.txt --templateFile ${DIRPREFIX}/ARM_1_2_3GHz_${FREQ}_X2.xml --numCores 3 > ${OUTXMLDIR}/${BENCH}_paramedic_1_2_3GHz_${FREQ}_X2_classic.xml # && \
        ${MCPATDIR}/mcpat -infile ${OUTXMLDIR}/${BENCH}_paramedic_1_2_3GHz_${FREQ}_X2_classic.xml -print_level 1 > ${OUTSTATDIR}/${BENCH}_paramedic_1_2_3GHz_${FREQ}_X2_classic.txt
    done
    for FREQ in 2000MHz 1800MHz 1600MHz 
    do
        echo "${BENCH}_paramedic_1_4_3GHz_${FREQ}_A510_classic_stats.txt"
        python3 ${DIRPREFIX}/mcpat_stat.py --statFile ${INSTATDIR}/${BENCH}_paramedic_1_4_3GHz_${FREQ}_A510_classic_stats.txt --templateFile ${DIRPREFIX}/ARM_1_4_3GHz_${FREQ}_A510.xml --numCores 5 > ${OUTXMLDIR}/${BENCH}_paramedic_1_4_3GHz_${FREQ}_A510_classic.xml && \
        ${MCPATDIR}/mcpat -infile ${OUTXMLDIR}/${BENCH}_paramedic_1_4_3GHz_${FREQ}_A510_classic.xml -print_level 1 > ${OUTSTATDIR}/${BENCH}_paramedic_1_4_3GHz_${FREQ}_A510_classic.txt
    done
done
