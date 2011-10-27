function readTMP(fileName)

fileName = 'output.tmp';
% fileName = 'highway2_seq1--1254417902.055041.tmp';
fid = fopen(fileName);
assert(fid>0);
frame = fread(fid, 1, 'int');
width = fread(fid, 1, 'int');
height = fread(fid, 1, 'int');
channel = fread(fid, 1, 'int');

datasize = height*width*channel;
% data = zeros(datasize,1);
data = fread(fid, inf, 'float'); assert(numel(data) == datasize);
data = reorderData(data, width, height, channel);

% figure(1);
% imshow(data(:,:,1:3));
% figure(2);
% imshow(data(:,:,4));
% figure(3);
% imshow(data(:,:,5));

fclose(fid);
end

function output = reorderData(data, width, height, channel)
    data = reshape(data, channel, []);
    output = zeros(height, width, channel);
    for i = 1:channel
        output(:,:,i) = reshape(data(i,:), width, height)';
    end
end
