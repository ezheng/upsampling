function main()

fileName1 = 'highway2_seq1--1254417902.055041.tmp';
fileName2 = 'output.tmp';

data1 = readTMP(fileName1);
data2 = readTMP(fileName2);

figure(1);
imshow((data1(:,:,1:3)))
figure(2);
imshow(data1(:,:,5));

figure(3);
imagesc(fliplr(data2'));
colormap(gray);
axis off; axis image;

