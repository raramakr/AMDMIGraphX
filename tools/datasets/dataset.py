import abc
import numpy as np
from datasets import load_dataset

from preprocess import process_image


class BaseDataset(object):
    __metaclass__ = abc.ABCMeta

    @abc.abstractclassmethod
    def __init__(self):
        pass

    @abc.abstractmethod
    def __iter__(self):
        pass

    @abc.abstractmethod
    def __next__(self):
        pass

    @abc.abstractmethod
    def transform(self, inputs, data, prepocess_fn):
        pass

    @abc.abstractmethod
    def name(self):
        pass


class ImageNet2012Val(BaseDataset):

    def __init__(self):
        self.url = "https://image-net.org/data/ILSVRC/2012/ILSVRC2012_img_val.tar"

    def __iter__(self):
        print(f"Load dataset from {self.url}")
        self.dataset = iter(
            load_dataset("webdataset",
                         data_files={"val": self.url},
                         split="val",
                         streaming=True))
        return self.dataset

    def __next__(self):
        return next(self.dataset)

    def transform(self, inputs, data, prepocess_fn):
        assert len(inputs) == 1
        img_data = prepocess_fn(data["jpeg"])
        assert (img_data.shape == (1, 3, 224, 224))
        return {inputs[0]: img_data}

    def name(self):
        return "imagenet-2012-val"


class SQuADBase(BaseDataset):

    @abc.abstractclassmethod
    def __init__(self):
        pass

    def __iter__(self):
        print(f"Load dataset from {self.url}")
        self.dataset = iter(
            load_dataset(self.url, split="validation", streaming=True))
        return self.dataset

    def __next__(self):
        return next(self.dataset)

    def transform(self, inputs, data, prepocess_fn):
        result = prepocess_fn(data["question"],
                              data["context"],
                              max_length=384)
        inputs, keys = sorted(inputs), sorted(list(result.keys()))
        assert inputs == keys, f"{inputs = } == {keys = }"
        # The result should be a simple dict, the preproc returns a wrapped class, dict() will remove it
        return dict(result)

    @abc.abstractclassmethod
    def name(self):
        return "squad-v1.1"


class SQuADv1_1(SQuADBase):

    def __init__(self):
        self.url = "https://raw.githubusercontent.com/rajpurkar/SQuAD-explorer/master/dataset/dev-v1.1.json"

    def __iter__(self):
        print(f"Load dataset from {self.url}")
        self.dataset = ({
            "context": paragraph["context"],
            "question": qas["question"]
        } for data in load_dataset("json",
                                   data_files={"val": self.url},
                                   split="val",
                                   field="data",
                                   streaming=True)
                        for paragraph in data["paragraphs"]
                        for qas in paragraph["qas"])
        return self.dataset

    def name(self):
        return "squad-v1.1"


class SQuAD_HF(SQuADBase):

    def __init__(self):
        self.url = "squad"

    def name(self):
        return "squad-hf"
