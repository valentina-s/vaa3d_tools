function obj = commitChanges(obj)
obj.data.spines([obj.data.spines.edittype]==1)=[];
% resort the axons according to distance
index=0;
for i=1:size(obj.data.dendrites,2)
    inds=find([obj.data.spines.den_ind]==i);
    [x,ord]=sort([obj.data.spines(inds).dendis]);
    for j=1:size(inds,2)
        obj.data.spines(inds(ord(j))).label=index+j;
    end
    index=index+size(inds,2);
end